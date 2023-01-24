use apollo_hyper_libretro_build::build;

fn main() {
    build("virtualjaguar-libretro", Some("virtualjaguar.patch"), None);
}
