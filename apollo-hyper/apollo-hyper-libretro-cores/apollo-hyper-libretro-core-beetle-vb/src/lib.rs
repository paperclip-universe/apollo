// TODO Replace when https://github.com/libretro/beetle-wswan-libretro/issues/93 is fixed
pub fn get_path() -> Option<&'static str> {
    let path = env!("COREPATH");
    if path != "none" {
        Some(path)
    } else {
        None
    }
}
