pub fn get_path() -> Option<&'static str> {
    let path = env!("COREPATH");
    if path != "none" {
        return Some(path);
    } else {
        return None;
    }
}
