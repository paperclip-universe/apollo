use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "snes9x/libretro",
        Some("snes9x.patch"),
        None,
        "https://github.com/libretro/snes9x",
    );
}
