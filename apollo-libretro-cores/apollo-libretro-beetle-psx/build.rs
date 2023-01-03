use apollo_libretro_build::build;

fn main() {
    build("beetle-psx-libretro", Some("mednafen_psx_hw.patch"));
}
