pub fn get_path() -> Option<&'static str> {
    Some(env!("CPATH"))
}
