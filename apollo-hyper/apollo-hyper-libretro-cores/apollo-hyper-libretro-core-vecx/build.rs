use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "libretro-vecx",
        Some("vecx.patch"),
        Some("Makefile.libretro"),
    );
}
