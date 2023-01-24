use apollo_hyper_libretro_build::build;

fn main() {
    build("yabause/yabause/src/libretro", Some("yabause.patch"), None);
}
