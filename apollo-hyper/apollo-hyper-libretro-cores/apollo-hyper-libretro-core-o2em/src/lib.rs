pub fn get_path() -> Option<&'static str> {
    let path = env!("COREPATH");
    if path != "none" {
        Some(path)
    } else {
        None
    }
}
