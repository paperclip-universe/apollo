use apollo_libretro_build::build;

fn main() {
    // #[cfg(not(target_os = "macos"))]
    build(
        "libretro-vecx",
        Some("vecx.patch"),
        Some("Makefile.libretro"),
    );
}
