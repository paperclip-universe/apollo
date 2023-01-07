#[cfg(not(target_os = "macos"))]
use apollo_libretro_build::build;

fn main() {
    #[cfg(not(target_os = "macos"))]
    build("beetle-wswan-libretro", Some("mednafen_wswan.patch"), None);
}
