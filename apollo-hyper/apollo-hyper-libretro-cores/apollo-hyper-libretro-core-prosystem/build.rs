use apollo_hyper_libretro_build::build;

fn main() {
    build("prosystem-libretro", Some("prosystem.patch"), None);
}
