use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "FreeChaF",
        Some("freechaf.pwtch"),
        None,
        "https://github.com/libretro/FreeChaF",
    );
}
