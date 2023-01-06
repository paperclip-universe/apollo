use apollo_libretro_build::build;

fn main() {
    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    build("parallel-n64", Some("parallel_n64.patch"), None);
}
