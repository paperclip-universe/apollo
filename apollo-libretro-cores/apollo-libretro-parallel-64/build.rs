use apollo_libretro_build::build;

fn main() {
    #[cfg(not(target_os = "windows"))]
    build("parallel-n64", Some("parallel_n64.patch"), None);
}
