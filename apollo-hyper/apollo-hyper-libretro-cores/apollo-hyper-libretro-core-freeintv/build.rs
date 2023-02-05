use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "FreeIntv",
        Some("freeintv.patch"),
        None,
        "https://github.com/libretro/freeintv",
    );
}
