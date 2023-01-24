// TODO Replace when https://github.com/libretro/beetle-wswan-libretro/issues/93 is fixed
pub fn get_path() -> Option<&'static str> {
    #[cfg(not(target_os = "macos"))]
    return Some(env!("CPATH"));
    #[cfg(target_os = "macos")]
    return None;
}
