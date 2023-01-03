use apollo_libretro_a5200::get_path as a5200_get_path;
use apollo_libretro_beetle_ngb::get_path as beetle_ngb_get_path;
use apollo_libretro_beetle_psx::get_path as beetle_psx_get_path;
use apollo_libretro_beetle_vb::get_path as beetle_vb_get_path;
use apollo_libretro_beetle_wswan::get_path as beetle_wswan_get_path;
use apollo_libretro_freechaf::get_path as freechaf_get_path;
use apollo_libretro_freeintv::get_path as freeintv_get_path;
use apollo_libretro_mgba::get_path as mgba_get_path;

pub fn get_all() -> Vec<&'static str> {
    const FUNCTIONS: [fn() -> Option<&'static str>; 8] = [
        a5200_get_path,
        beetle_ngb_get_path,
        beetle_psx_get_path,
        beetle_vb_get_path,
        beetle_wswan_get_path,
        freechaf_get_path,
        freeintv_get_path,
        mgba_get_path,
    ];

    FUNCTIONS.into_iter().map(|x| x()).flatten().collect()
}

#[test]
fn it_works() {
    dbg!(get_all());
}
