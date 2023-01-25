use apollo_hyper_libretro_build::build;

fn main() {
    build("beetle-ngp-libretro", Some("mednafen_psx_hw.patch"), None);
}
