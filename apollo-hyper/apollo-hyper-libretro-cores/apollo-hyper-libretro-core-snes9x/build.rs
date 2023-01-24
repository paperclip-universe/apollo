use apollo_hyper_libretro_build::build;

fn main() {
    build("snes9x/libretro", Some("opera.patch"), None);
}
