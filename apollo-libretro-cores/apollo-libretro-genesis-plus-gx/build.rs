use apollo_libretro_build::build;

fn main() {
    build(
        "Genesis-Plus-GX",
        Some("genesis_plus_gx.patch"),
        Some("Makefile.libretro"),
    );
}
