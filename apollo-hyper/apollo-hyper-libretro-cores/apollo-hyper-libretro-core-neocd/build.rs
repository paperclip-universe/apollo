use apollo_hyper_libretro_build::build;

fn main() {
    build("neocd_libretro", Some("neocd.patch"), None);
}
