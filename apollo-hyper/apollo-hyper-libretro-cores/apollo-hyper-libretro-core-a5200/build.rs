use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "a5200",
        Some("a5200.patch"),
        None,
        "https://github.com/libretro/a5200",
    );
}
