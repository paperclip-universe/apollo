use apollo_libretro_mgba::get_path as mgba_get_path;

pub fn get_all() -> Vec<&'static str> {
    vec![mgba_get_path()]
}

#[test]
fn it_works() {
    dbg!(get_all());
}
