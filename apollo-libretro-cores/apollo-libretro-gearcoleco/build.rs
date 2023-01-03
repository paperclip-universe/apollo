use apollo_libretro_build::build;

fn main() {
    build(
        "Gearcoleco/platforms/libretro",
        Some("gearcoleco.pwtch"),
        None,
    );
}
