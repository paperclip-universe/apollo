use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "stella2014-libretro",
        Some("stella2014.patch"),
        None,
        "https://github.com/libretro/stella2014-libretro",
    );
}
