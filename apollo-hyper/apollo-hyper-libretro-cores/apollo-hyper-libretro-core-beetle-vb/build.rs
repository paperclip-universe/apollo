#[cfg(not(target_os = "macos"))]
use apollo_hyper_libretro_build::build;

fn main() {
    #[cfg(not(target_os = "macos"))]
    build("beetle-vb-libretro", Some("mednafen_vb.patch"), None);
}
