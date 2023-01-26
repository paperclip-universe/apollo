use apollo_hyper_libretro_build::build;

fn main() {
    build("nestopia/libretro", Some("nestopia.patch"), None);
}
