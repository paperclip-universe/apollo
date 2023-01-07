pub fn get_path() -> Option<&'static str> {
    return Some(env!("CPATH"));
}
