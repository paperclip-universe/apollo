pub fn get_path() -> Option<&'static str> {
    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    return Some(env!("CPATH"));
    #[cfg(any(target_os = "windows", target_os = "macos"))]
    return None;
}
