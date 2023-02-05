use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "libretro-handy",
        Some("handy.patch"),
        None,
        "https://github.com/libretro/libretro-handy",
    );
}
