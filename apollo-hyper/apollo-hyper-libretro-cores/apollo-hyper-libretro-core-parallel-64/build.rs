use apollo_hyper_libretro_build::build;

fn main() {
    build(
        "parallel-n64",
        Some("parallel_n64.patch"),
        None,
        "https://github.com/libretro/parallel-n64",
    );
}
