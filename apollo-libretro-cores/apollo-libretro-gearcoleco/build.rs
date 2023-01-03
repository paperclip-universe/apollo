use apollo_libretro_build::build;

fn main() {
    build("Gearcoleco", Some("gearcoleco.pwtch"), Some("./platforms/libretro/Makefile"));
}
