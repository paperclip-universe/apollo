use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "Gearcoleco/platforms/libretro",
        Some("gearcoleco.patch"),
        None,
    );
}
