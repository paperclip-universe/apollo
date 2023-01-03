use crate::{buttons::InputPort, error::*};
use apollo_standard::{ApolloEmulator, ApolloMultiEmulator};
use libc::c_char;
use libloading::{Library, Symbol};
use libretro_sys::*;
use std::{
    ffi::{c_void, CStr, CString},
    fs::File,
    io::Read,
    marker::PhantomData,
    panic,
    path::{Path, PathBuf},
    ptr,
};

type NotSendSync = *const [u8; 0];

static mut EMULATOR: *mut EmulatorCore = ptr::null_mut();
static mut CONTEXT: *mut EmulatorContext = ptr::null_mut();

struct EmulatorCore {
    core_lib: Box<Library>,
    #[allow(dead_code)]
    core_path: CString,
    system_path: CString,
    rom_path: CString,
    core: CoreAPI,
    _marker: PhantomData<NotSendSync>,
}

struct EmulatorContext {
    audio_sample: Vec<i16>,
    input_ports: [InputPort; 2],
    frame_ptr: *const c_void,
    frame_pitch: usize,
    frame_width: u32,
    frame_height: u32,
    pixfmt: PixelFormat,
    image_depth: usize,
    memory_map: Vec<MemoryDescriptor>,
    _marker: PhantomData<NotSendSync>,
}

// A more pleasant wrapper over MemoryDescriptor
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct MemoryRegion {
    which: usize,
    pub flags: u64,
    pub len: usize,
    pub start: usize,
    pub offset: usize,
    pub name: String,
    pub select: usize,
    pub disconnect: usize,
}

// Emulator token must not be send nor sync
pub struct Emulator {
    phantom: PhantomData<NotSendSync>,
    system_info: SystemInfo,
    system_av_info: SystemAvInfo,
}

pub struct MultiEmulator {}

pub struct RetroSystemInfo {
    pub library_name: CString,
    pub extensions: CString,
}

fn create_core(core_path: &Path) -> (Box<Library>, CoreAPI) {
    let suffix = if cfg!(target_os = "windows") {
        "dll"
    } else if cfg!(target_os = "macos") {
        "dylib"
    } else if cfg!(target_os = "linux") {
        "so"
    } else {
        panic!("Unsupported platform")
    };
    let path: PathBuf = core_path.with_extension(suffix);
    #[cfg(target_os = "linux")]
    let library: Library = unsafe {
        // Load library with `RTLD_NOW | RTLD_NODELETE` to fix a SIGSEGV
        ::libloading::os::unix::Library::open(Some(path), 0x2 | 0x1000)
            .unwrap()
            .into()
    };

    #[cfg(not(target_os = "linux"))]
    let library = unsafe { Library::new(path).unwrap() };
    let dll = Box::new(library);

    unsafe {
        let retro_set_environment = *(dll.get(b"retro_set_environment").unwrap());
        let retro_set_video_refresh = *(dll.get(b"retro_set_video_refresh").unwrap());
        let retro_set_audio_sample = *(dll.get(b"retro_set_audio_sample").unwrap());
        let retro_set_audio_sample_batch = *(dll.get(b"retro_set_audio_sample_batch").unwrap());
        let retro_set_input_poll = *(dll.get(b"retro_set_input_poll").unwrap());
        let retro_set_input_state = *(dll.get(b"retro_set_input_state").unwrap());
        let retro_init = *(dll.get(b"retro_init").unwrap());
        let retro_deinit = *(dll.get(b"retro_deinit").unwrap());
        let retro_api_version = *(dll.get(b"retro_api_version").unwrap());
        let retro_get_system_info = *(dll.get(b"retro_get_system_info").unwrap());
        let retro_get_system_av_info = *(dll.get(b"retro_get_system_av_info").unwrap());
        let retro_set_controller_port_device =
            *(dll.get(b"retro_set_controller_port_device").unwrap());
        let retro_reset = *(dll.get(b"retro_reset").unwrap());
        let retro_run = *(dll.get(b"retro_run").unwrap());
        let retro_serialize_size = *(dll.get(b"retro_serialize_size").unwrap());
        let retro_serialize = *(dll.get(b"retro_serialize").unwrap());
        let retro_unserialize = *(dll.get(b"retro_unserialize").unwrap());
        let retro_cheat_reset = *(dll.get(b"retro_cheat_reset").unwrap());
        let retro_cheat_set = *(dll.get(b"retro_cheat_set").unwrap());
        let retro_load_game = *(dll.get(b"retro_load_game").unwrap());
        let retro_load_game_special = *(dll.get(b"retro_load_game_special").unwrap());
        let retro_unload_game = *(dll.get(b"retro_unload_game").unwrap());
        let retro_get_region = *(dll.get(b"retro_get_region").unwrap());
        let retro_get_memory_data = *(dll.get(b"retro_get_memory_data").unwrap());
        let retro_get_memory_size = *(dll.get(b"retro_get_memory_size").unwrap());
        let core = CoreAPI {
            retro_set_environment,
            retro_set_video_refresh,
            retro_set_audio_sample,
            retro_set_audio_sample_batch,
            retro_set_input_poll,
            retro_set_input_state,

            retro_init,
            retro_deinit,

            retro_api_version,

            retro_get_system_info,
            retro_get_system_av_info,
            retro_set_controller_port_device,

            retro_reset,
            retro_run,

            retro_serialize_size,
            retro_serialize,
            retro_unserialize,

            retro_cheat_reset,
            retro_cheat_set,

            retro_load_game,
            retro_load_game_special,
            retro_unload_game,

            retro_get_region,
            retro_get_memory_data,
            retro_get_memory_size,
        };
        (dll, core)
    }
}

impl ApolloMultiEmulator for MultiEmulator {
    // FIXME Move some of this logic to Emulator::boot
    fn create_emulator(core_path: &Path, rom_path: &Path) -> Emulator {
        unsafe {
            assert!(EMULATOR.is_null());
            assert!(CONTEXT.is_null());
        }

        let (dll, core) = create_core(core_path);

        let emu = EmulatorCore {
            core_lib: dll,
            rom_path: CString::new(rom_path.to_str().unwrap()).unwrap(),
            core_path: CString::new(core_path.to_str().unwrap()).unwrap(),
            system_path: { CString::new(core_path.with_extension("").to_str().unwrap()).unwrap() },
            core: core.clone(),
            _marker: PhantomData,
        };

        unsafe {
            let emup = Box::new(emu);
            // Store a pointer to the data
            EMULATOR = Box::leak(emup);
            // Forget the box so it doesn't drop
            let ctx = EmulatorContext {
                audio_sample: Vec::new(),
                input_ports: [InputPort::new(), InputPort::new()],
                frame_ptr: ptr::null(),
                frame_pitch: 0,
                frame_width: 0,
                frame_height: 0,
                pixfmt: PixelFormat::ARGB1555,
                image_depth: 0,
                memory_map: Vec::new(),
                _marker: PhantomData,
            };
            // Ditto here for the context
            let ctxp = Box::new(ctx);
            CONTEXT = Box::leak(ctxp);
            let emu = &(*EMULATOR);
            // Set up callbacks
            (emu.core.retro_set_environment)(callback_environment);
            (emu.core.retro_set_video_refresh)(callback_video_refresh);
            (emu.core.retro_set_audio_sample)(callback_audio_sample);
            (emu.core.retro_set_audio_sample_batch)(callback_audio_sample_batch);
            (emu.core.retro_set_input_poll)(callback_input_poll);
            (emu.core.retro_set_input_state)(callback_input_state);
            // Load the game
            (emu.core.retro_init)();
            let rom_cstr = &(*EMULATOR).rom_path;

            let mut rom_file = File::open(rom_path).unwrap();
            let mut buffer = Vec::new();
            rom_file.read_to_end(&mut buffer).unwrap();
            buffer.shrink_to_fit();
            let game_info = GameInfo {
                path: rom_cstr.as_ptr(),
                data: buffer.as_ptr() as *const c_void,
                size: buffer.len(),
                meta: ptr::null(),
            };

            let load_game_successful = (emu.core.retro_load_game)(&game_info);
            assert!(load_game_successful);

            let mut system_info = SystemInfo {
                library_name: ptr::null(),
                library_version: ptr::null(),
                valid_extensions: ptr::null(),
                need_fullpath: false,
                block_extract: false,
            };
            let mut system_av_info = SystemAvInfo {
                geometry: GameGeometry {
                    base_width: 0,
                    base_height: 0,
                    max_width: 0,
                    max_height: 0,
                    aspect_ratio: 0.0,
                },
                timing: SystemTiming {
                    fps: 0.0,
                    sample_rate: 0.0,
                },
            };

            (core.retro_get_system_info)(&mut system_info);
            (core.retro_get_system_av_info)(&mut system_av_info);

            Emulator {
                phantom: PhantomData,
                system_info,
                system_av_info,
            }
        }
    }
}

impl ApolloEmulator for Emulator {
    fn boot(&self, _: &Path) {}
}

impl Emulator {
    pub fn create_for_system_info(core_path: &Path) -> RetroSystemInfo {
        let (_, core) = create_core(core_path);
        let mut system_info = SystemInfo {
            library_name: ptr::null(),
            library_version: ptr::null(),
            valid_extensions: ptr::null(),
            need_fullpath: false,
            block_extract: false,
        };

        let to_cstring = |ptr| unsafe { CStr::from_ptr(ptr).to_owned() };

        unsafe {
            (core.retro_get_system_info)(&mut system_info);
            RetroSystemInfo {
                library_name: to_cstring(system_info.library_name),
                extensions: to_cstring(system_info.valid_extensions),
            }
        }
    }

    pub fn get_library(&mut self) -> &Library {
        unsafe { &(*EMULATOR).core_lib }
    }
    pub fn get_symbol<'a, T>(&'a self, symbol: &[u8]) -> Option<Symbol<'a, T>> {
        let dll = unsafe { &(*EMULATOR).core_lib };
        let sym: Result<Symbol<T>, _> = unsafe { dll.get(symbol) };
        if sym.is_err() {
            return None;
        }
        Some(sym.unwrap())
    }
    pub fn run(&mut self, inputs: [InputPort; 2]) {
        unsafe {
            //clear audio buffers and whatever else
            (*CONTEXT).audio_sample.clear();
            //set inputs on CB
            (*CONTEXT).input_ports = inputs;
            //run one step
            ((*EMULATOR).core.retro_run)()
        }
    }
    pub fn reset(&mut self) {
        unsafe {
            //clear audio buffers and whatever else
            (*CONTEXT).audio_sample.clear();
            //clear inputs on CB
            (*CONTEXT).input_ports = [InputPort::new(), InputPort::new()];
            //clear fb
            (*CONTEXT).frame_ptr = ptr::null();
            ((*EMULATOR).core.retro_reset)()
        }
    }
    fn get_ram_size(&self, rtype: libc::c_uint) -> usize {
        unsafe { ((*EMULATOR).core.retro_get_memory_size)(rtype) }
    }
    pub fn get_video_ram_size(&self) -> usize {
        self.get_ram_size(MEMORY_VIDEO_RAM)
    }
    pub fn get_system_ram_size(&self) -> usize {
        self.get_ram_size(MEMORY_SYSTEM_RAM)
    }
    pub fn get_save_ram_size(&self) -> usize {
        self.get_ram_size(MEMORY_SAVE_RAM)
    }
    pub fn video_ram_ref(&self) -> &[u8] {
        self.get_ram(MEMORY_VIDEO_RAM)
    }
    pub fn system_ram_ref(&self) -> &[u8] {
        self.get_ram(MEMORY_SYSTEM_RAM)
    }
    pub fn system_ram_mut(&mut self) -> &mut [u8] {
        self.get_ram_mut(MEMORY_SYSTEM_RAM)
    }
    pub fn save_ram(&self) -> &[u8] {
        self.get_ram(MEMORY_SAVE_RAM)
    }

    fn get_ram(&self, ramtype: libc::c_uint) -> &[u8] {
        let len = self.get_ram_size(ramtype);
        unsafe {
            let ptr: *const u8 = ((*EMULATOR).core.retro_get_memory_data)(ramtype).cast();
            std::slice::from_raw_parts(ptr, len)
        }
    }

    fn get_ram_mut(&mut self, ramtype: libc::c_uint) -> &mut [u8] {
        let len = self.get_ram_size(ramtype);
        unsafe {
            let ptr: *mut u8 = ((*EMULATOR).core.retro_get_memory_data)(ramtype).cast();
            std::slice::from_raw_parts_mut(ptr, len)
        }
    }

    pub fn memory_regions(&self) -> Vec<MemoryRegion> {
        let map = unsafe { &((*CONTEXT).memory_map) };
        map.iter()
            .enumerate()
            .map(|(i, mdesc)| MemoryRegion {
                which: i,
                flags: mdesc.flags,
                len: mdesc.len,
                start: mdesc.start,
                offset: mdesc.offset,
                select: mdesc.select,
                disconnect: mdesc.disconnect,
                name: if mdesc.addrspace.is_null() {
                    "".to_owned()
                } else {
                    unsafe { CStr::from_ptr(mdesc.addrspace) }
                        .to_string_lossy()
                        .into_owned()
                },
            })
            .collect()
    }
    pub fn memory_ref(&self, start: usize) -> Result<&[u8], RetroRsError> {
        for mr in self.memory_regions() {
            if mr.select != 0 && (start & mr.select) == 0 {
                continue;
            }
            if start >= mr.start && start < mr.start + mr.len {
                return self.memory_ref_mut(mr, start).map(|slice| &*slice);
            }
        }
        Err(RetroRsError::RAMCopyNotMappedIntoMemoryRegionError)
    }
    pub fn memory_ref_mut(
        &self,
        mr: MemoryRegion,
        start: usize,
    ) -> Result<&mut [u8], RetroRsError> {
        let maps = unsafe { &(*CONTEXT).memory_map };
        if mr.which >= maps.len() {
            // TODO more aggressive checking of mr vs map
            return Err(RetroRsError::RAMMapOutOfRangeError);
        }
        if start < mr.start {
            return Err(RetroRsError::RAMCopySrcOutOfBoundsError);
        }
        let start = (start - mr.start) & !mr.disconnect;
        let map = &maps[mr.which];
        //0-based at this point, modulo offset
        let ptr: *mut u8 = map.ptr.cast();
        let slice = unsafe {
            let ptr = ptr.add(start).add(map.offset);
            std::slice::from_raw_parts_mut(ptr, map.len - start)
        };
        Ok(slice)
    }

    pub fn pixel_format(&self) -> PixelFormat {
        unsafe { (*CONTEXT).pixfmt }
    }
    pub fn framebuffer_size(&self) -> (usize, usize) {
        unsafe {
            (
                (*CONTEXT).frame_width as usize,
                (*CONTEXT).frame_height as usize,
            )
        }
    }
    pub fn framebuffer_pitch(&self) -> usize {
        unsafe { (*CONTEXT).frame_pitch }
    }
    pub fn peek_framebuffer<FBPeek, FBPeekRet>(&self, f: FBPeek) -> Result<FBPeekRet, RetroRsError>
    where
        FBPeek: FnOnce(&[u8]) -> FBPeekRet,
    {
        unsafe {
            if (*CONTEXT).frame_ptr.is_null() {
                Err(RetroRsError::NoFramebufferError)
            } else {
                let frame_slice = std::slice::from_raw_parts(
                    (*CONTEXT).frame_ptr as *const u8,
                    ((*CONTEXT).frame_height * ((*CONTEXT).frame_pitch as u32)) as usize,
                );
                Ok(f(frame_slice))
            }
        }
    }

    pub fn peek_audio_buffer<F, R>(&self, f: F) -> Result<R, RetroRsError>
    where
        F: FnOnce(&[i16]) -> R,
    {
        unsafe {
            let slice = &(*CONTEXT).audio_sample[..];
            Ok(f(slice))
        }
    }

    pub fn save(&self, bytes: &mut [u8]) {
        let size = self.save_size();
        assert!(bytes.len() >= size);
        unsafe { ((*EMULATOR).core.retro_serialize)(bytes.as_mut_ptr() as *mut c_void, size) }
    }
    pub fn load(&mut self, bytes: &[u8]) -> bool {
        let size = self.save_size();
        assert!(bytes.len() >= size);
        unsafe { ((*EMULATOR).core.retro_unserialize)(bytes.as_ptr() as *const c_void, size) }
    }
    pub fn save_size(&self) -> usize {
        unsafe { ((*EMULATOR).core.retro_serialize_size)() }
    }
    pub fn clear_cheats(&mut self) {
        unsafe { ((*EMULATOR).core.retro_cheat_reset)() }
    }
    pub fn set_cheat(&mut self, index: usize, enabled: bool, code: &str) {
        unsafe {
            // FIXME: Creates a memory leak since the libretro api won't let me from_raw() it back and drop it.  I don't know if libretro guarantees anything about ownership of this str to cores.
            ((*EMULATOR).core.retro_cheat_set)(
                index as u32,
                enabled,
                CString::new(code).unwrap().into_raw(),
            )
        }
    }

    pub fn system_info(&self) -> &SystemInfo {
        &self.system_info
    }

    pub fn system_av_info(&self) -> &SystemAvInfo {
        &self.system_av_info
    }
}

unsafe extern "C" fn callback_environment(cmd: u32, data: *mut c_void) -> bool {
    let result = panic::catch_unwind(|| {
        match cmd {
            ENVIRONMENT_SET_CONTROLLER_INFO => true,
            ENVIRONMENT_SET_PIXEL_FORMAT => {
                let pixfmti = *(data as *const u32);
                let pixfmt = PixelFormat::from_uint(pixfmti);
                if pixfmt.is_none() {
                    return false;
                }
                let pixfmt = pixfmt.unwrap();
                (*CONTEXT).image_depth = match pixfmt {
                    PixelFormat::ARGB1555 => 15,
                    PixelFormat::ARGB8888 => 32,
                    PixelFormat::RGB565 => 16,
                };
                (*CONTEXT).pixfmt = pixfmt;
                true
            }
            ENVIRONMENT_GET_SYSTEM_DIRECTORY => {
                *(data as *mut *const c_char) = (*EMULATOR).system_path.as_ptr();
                println!("{}", (*EMULATOR).system_path.to_str().unwrap());
                true
            }
            ENVIRONMENT_GET_CAN_DUPE => {
                *(data as *mut bool) = true;
                true
            }
            ENVIRONMENT_SET_MEMORY_MAPS => {
                let map = data as *const MemoryMap;
                let desc_slice =
                    std::slice::from_raw_parts((*map).descriptors, (*map).num_descriptors as usize);
                // Don't know who owns map or how long it will last
                (*CONTEXT).memory_map = Vec::new();
                // So we had better copy it
                (*CONTEXT).memory_map.extend_from_slice(desc_slice);
                // (Implicitly we also want to drop the old one, which we did by reassigning)
                true
            }

            // Added by Aldo Acevedo (2022)
            ENVIRONMENT_GET_VARIABLE => {
                let variable = data as *const Variable;
                let key = CStr::from_ptr((*(variable)).key).to_str().unwrap();

                let value = match key {
                    //"pcsx2_bios" => Some("SCPH-39001 NA 160-mar.bin"),
                    "pcsx2_fastboot" => Some("disabled"),
                    "pcsx2_memcard_slot_1" => Some("shared8"),
                    "pcsx2_memcard_slot_2" => Some("empty"),
                    "pcsx2_renderer" => Some("Auto"),

                    "pcsx2_upscale_multiplier" => Some("1"),
                    "pcsx2_palette_conversion" => Some("disabled"),
                    "pcsx2_userhack_fb_conversion" => Some("disabled"),
                    "pcsx2_userhack_auto_flush" => Some("disabled"),
                    "pcsx2_fast_invalidation" => Some("disabled"),
                    "pcsx2_speedhacks_presets" => Some("1"),
                    "pcsx2_fastcdvd" => Some("disabled"),
                    "pcsx2_deinterlace_mode" => Some("7"),
                    "pcsx2_enable_60fps_patches" => Some("disabled"),
                    "pcsx2_enable_widescreen_patches" => Some("disabled"),
                    "pcsx2_frameskip" => Some("disabled"),
                    "pcsx2_frames_to_draw" => Some("1"),
                    "pcsx2_frames_to_skip" => Some("1"),
                    "pcsx2_vsync_mtgs_queue" => Some("2"),
                    "pcsx2_enable_cheats" => Some("disabled"),
                    "pcsx2_clamping_mode" => Some("1"),
                    "pcsx2_round_mode" => Some("3"),
                    "pcsx2_vu_clamping_mode" => Some("1"),
                    "pcsx2_vu_round_mode" => Some("3"),
                    "pcsx2_gamepad_l_deadzone" => Some("0"),
                    "pcsx2_gamepad_r_deadzone" => Some("0"),
                    "pcsx2_rumble_intensity" => Some("100"),
                    "pcsx2_rumble_enable" => Some("enabled"),

                    "beetle_psx_renderer" => Some("software"),
                    "beetle_psx_hw_renderer" => Some("software"),

                    "desmume_pointer_mouse" => Some("enabled"),
                    "desmume_pointer_device_l" => Some("absolute"),
                    "desmume_pointer_device_r" => Some("absolute"),
                    //"desmume_pointer_type" => Some("touch"),
                    key => {
                        eprintln!("INTERNAL: Unrecognized variable: {key}");
                        None
                    }
                };

                // Successful if value isn't null
                value.is_some()
            }
            ENVIRONMENT_GET_LOG_INTERFACE => {
                // Variadic C print function
                unsafe extern "C" fn log_function(
                    level: LogLevel,
                    fmt: *const libc::c_char,
                    mut args: ...
                ) {
                    let level = match level {
                        LogLevel::Debug => "DEBUG",
                        LogLevel::Info => "INFO",
                        LogLevel::Warn => "WARN",
                        LogLevel::Error => "ERROR",
                    };
                    let format_args = printf_compat::output::display(fmt, args.as_va_list());
                    eprint!("{level}: {format_args}");
                }

                let log_callback = data as *mut LogCallback;
                (*log_callback).log = log_function;
                true
            }
            ENVIRONMENT_SET_MESSAGE => {
                let msg = data as *const Message;
                eprint!(
                    "--- MESSAGE: {}",
                    CStr::from_ptr((*msg).msg).to_str().unwrap()
                );
                true
            }
            _ => false,
        }
    });
    result.unwrap_or(false)
}

extern "C" fn callback_video_refresh(data: *const c_void, width: u32, height: u32, pitch: usize) {
    // Can't panic
    unsafe {
        // context's framebuffer just points to the given data.  Seems to work OK for gym-retro.
        if !data.is_null() {
            (*CONTEXT).frame_ptr = data;
            (*CONTEXT).frame_pitch = pitch;
            (*CONTEXT).frame_width = width;
            (*CONTEXT).frame_height = height;
        }
    }
}
extern "C" fn callback_audio_sample(left: i16, right: i16) {
    // Can't panic
    unsafe {
        let sample_buf = &mut (*CONTEXT).audio_sample;
        sample_buf.push(left);
        sample_buf.push(right);
    }
}
extern "C" fn callback_audio_sample_batch(data: *const i16, frames: usize) -> usize {
    // Can't panic
    unsafe {
        let sample_buf = &mut (*CONTEXT).audio_sample;
        let slice = std::slice::from_raw_parts(data, frames * 2);
        sample_buf.clear();
        sample_buf.extend_from_slice(slice);
        frames
    }
}

extern "C" fn callback_input_poll() {}

extern "C" fn callback_input_state(port: u32, device: u32, index: u32, id: u32) -> i16 {
    if port > 1 {
        println!("Unsupported port {port} (only two controllers are implemented)");
        return 0;
    }

    let port = port as usize;

    match (device, index, id) {
        // RETRO_DEVICE_NONE         0
        (DEVICE_NONE, ..) => 0,
        // RETRO_DEVICE_JOYPAD       1
        (DEVICE_JOYPAD, 0, id) => {
            if id > 16 {
                println!("Unexpected button id {id}");
                return 0;
            }
            unsafe {
                if (*CONTEXT).input_ports[port].buttons.get(id) {
                    1
                } else {
                    0
                }
            }
        }
        // RETRO_DEVICE_MOUSE        2
        (DEVICE_MOUSE, 0, id) => unsafe {
            println!("MOUSE REQUESTED!!!!");
            let left = (*CONTEXT).input_ports[port].mouse_left_down;
            let right = (*CONTEXT).input_ports[port].mouse_right_down;
            let middle = (*CONTEXT).input_ports[port].mouse_middle_down;
            let bool_to_i16 = |b| if b { 1 } else { 0 };

            match id {
                DEVICE_ID_MOUSE_X => (*CONTEXT).input_ports[port].mouse_x,
                DEVICE_ID_MOUSE_Y => (*CONTEXT).input_ports[port].mouse_y,
                DEVICE_ID_MOUSE_LEFT => bool_to_i16(left),
                DEVICE_ID_MOUSE_RIGHT => bool_to_i16(right),
                DEVICE_ID_MOUSE_MIDDLE => bool_to_i16(middle),
                _ => 0,
            }
        },
        // RETRO_DEVICE_KEYBOARD     3
        (DEVICE_KEYBOARD, ..) => {
            println!("Keyboard input method unsupported");
            0
        }
        // RETRO_DEVICE_LIGHTGUN     4
        (DEVICE_LIGHTGUN, ..) => {
            println!("Lightgun input method unsupported");
            0
        }
        // RETRO_DEVICE_ANALOG       5
        //(DEVICE_ANALOG, DEVICE_INDEX_ANALOG_LEFT, DEVICE_ID_JOYPAD_X) => unsafe { (*CONTEXT).input_ports[port].joystick_x },
        //(DEVICE_ANALOG, DEVICE_INDEX_ANALOG_LEFT, DEVICE_ID_JOYPAD_Y) => unsafe { (*CONTEXT).input_ports[port].joystick_y },
        // Right not yet implemented
        (DEVICE_ANALOG, DEVICE_INDEX_ANALOG_LEFT, DEVICE_ID_ANALOG_X) => unsafe {
            (*CONTEXT).input_ports[port].joystick_x
        },
        (DEVICE_ANALOG, DEVICE_INDEX_ANALOG_LEFT, DEVICE_ID_ANALOG_Y) => unsafe {
            (*CONTEXT).input_ports[port].joystick_y
        },
        // RETRO_DEVICE_POINTER      6
        (6, ..) => {
            println!("Pointer input method unsupported");
            0
        }
        _ => {
            println!("Unsupported device/index/id ({device}/{index}/{id})");
            0
        }
    }
}

impl Drop for Emulator {
    fn drop(&mut self) {
        unsafe {
            ((*EMULATOR).core.retro_unload_game)();
            ((*EMULATOR).core.retro_deinit)();
        }
        //TODO drop memory maps etc
        unsafe {
            // "remember" context and emulator we forgot before
            let _ctx = Box::from_raw(CONTEXT);
            let _emu = Box::from_raw(EMULATOR);
            CONTEXT = ptr::null_mut();
            EMULATOR = ptr::null_mut();
        }
        // let them drop naturally
    }
}
