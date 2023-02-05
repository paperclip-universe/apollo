use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "mupen64plus-libretro-nx",
        Some("mupen64plus_next.patch"),
        None,
        "https://github.com/libretro/mupen64plus-libretro-nx",
    );
}
