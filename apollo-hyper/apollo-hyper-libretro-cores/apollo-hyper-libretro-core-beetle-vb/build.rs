use apollo_hyper_libretro_build::build;

fn main() {
    build("beetle-vb-libretro", Some("mednafen_vb.patch"), None);
}
