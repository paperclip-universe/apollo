use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "opera-libretro",
        Some("opera.patch"),
        None,
        "https://github.com/libretro/opera-libretro",
    );
}
