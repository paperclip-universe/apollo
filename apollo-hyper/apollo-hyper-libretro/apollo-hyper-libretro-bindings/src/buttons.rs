use apollo_hyper_libretro_sys::{
    DEVICE_ID_JOYPAD_A, DEVICE_ID_JOYPAD_B, DEVICE_ID_JOYPAD_DOWN, DEVICE_ID_JOYPAD_L,
    DEVICE_ID_JOYPAD_L2, DEVICE_ID_JOYPAD_L3, DEVICE_ID_JOYPAD_LEFT, DEVICE_ID_JOYPAD_R,
    DEVICE_ID_JOYPAD_R2, DEVICE_ID_JOYPAD_R3, DEVICE_ID_JOYPAD_RIGHT, DEVICE_ID_JOYPAD_SELECT,
    DEVICE_ID_JOYPAD_START, DEVICE_ID_JOYPAD_UP, DEVICE_ID_JOYPAD_X, DEVICE_ID_JOYPAD_Y,
};

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Default, Debug)]
pub struct InputPort {
    pub buttons: Buttons,
    pub joystick_x: i16,
    pub joystick_y: i16,
    pub mouse_x: i16,
    pub mouse_y: i16,
    pub mouse_left_down: bool,
    pub mouse_right_down: bool,
    pub mouse_middle_down: bool,
}

impl InputPort {
    pub fn new() -> Self {
        InputPort::default()
    }
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Default, Debug)]
pub struct Buttons(i16);

impl Buttons {
    pub fn new() -> Self {
        Buttons::default()
    }
    pub fn get(self, id: u32) -> bool {
        assert!(id < 16);
        (self.0 & (1 << id)) != 0
    }
    fn mask_inout(self, b: bool, id: u32) -> Self {
        assert!(id < 16);
        if b {
            Buttons(self.0 | (1 << id))
        } else {
            Buttons(self.0 & !(1 << id))
        }
    }
    pub fn up(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_UP)
    }
    pub fn down(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_DOWN)
    }
    pub fn left(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_LEFT)
    }
    pub fn right(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_RIGHT)
    }

    pub fn select(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_SELECT)
    }
    pub fn start(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_START)
    }
    pub fn a(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_A)
    }
    pub fn b(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_B)
    }
    pub fn y(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_Y)
    }
    pub fn x(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_X)
    }
    pub fn l1(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_L)
    }
    pub fn r1(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_R)
    }
    pub fn l2(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_L2)
    }
    pub fn r2(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_R2)
    }
    pub fn l3(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_L3)
    }
    pub fn r3(self, b: bool) -> Self {
        self.mask_inout(b, DEVICE_ID_JOYPAD_R3)
    }

    pub fn get_up(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_UP)
    }
    pub fn get_down(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_DOWN)
    }
    pub fn get_left(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_LEFT)
    }
    pub fn get_right(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_RIGHT)
    }

    pub fn get_select(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_SELECT)
    }
    pub fn get_start(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_START)
    }
    pub fn get_a(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_A)
    }
    pub fn get_b(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_B)
    }
    pub fn get_y(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_Y)
    }
    pub fn get_x(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_X)
    }
    pub fn get_l1(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_L)
    }
    pub fn get_r1(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_R)
    }
    pub fn get_l2(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_L2)
    }
    pub fn get_r2(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_R2)
    }
    pub fn get_l3(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_L3)
    }
    pub fn get_r3(self) -> bool {
        self.get(DEVICE_ID_JOYPAD_R3)
    }
}
