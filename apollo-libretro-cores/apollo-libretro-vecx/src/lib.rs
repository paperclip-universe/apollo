pub fn get_path() -> Option<&'static str> {
    #[cfg(not(target_os = "macos"))]
    Some(env!("CPATH"))
    #[cfg(target_os = "macos")]
    None
}
