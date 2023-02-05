use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "libretro-o2em",
        Some("o2em.patch"),
        None,
        "https://github.com/libretro/libretro-o2em",
    );
}
