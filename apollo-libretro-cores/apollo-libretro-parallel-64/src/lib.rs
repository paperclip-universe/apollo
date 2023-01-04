pub fn get_path() -> Option<&'static str> {
    #[cfg(not(target_os = "windows"))]
    return Some(env!("CPATH"));
    #[cfg(target_os = "macos")]
    return None;
}
