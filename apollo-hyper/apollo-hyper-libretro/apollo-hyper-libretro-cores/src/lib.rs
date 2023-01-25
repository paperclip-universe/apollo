use apollo_hyper_libretro_core_a5200::get_path as a5200_get_path;
use apollo_hyper_libretro_core_beetle_ngp::get_path as beetle_ngp_get_path;
use apollo_hyper_libretro_core_beetle_psx::get_path as beetle_psx_get_path;
use apollo_hyper_libretro_core_beetle_vb::get_path as beetle_vb_get_path;
use apollo_hyper_libretro_core_beetle_wswan::get_path as beetle_wswan_get_path;
use apollo_hyper_libretro_core_freechaf::get_path as freechaf_get_path;
use apollo_hyper_libretro_core_freeintv::get_path as freeintv_get_path;
use apollo_hyper_libretro_core_gearcoleco::get_path as gearcoleco_get_path;
use apollo_hyper_libretro_core_genesis_plus_gx::get_path as genesis_plus_gx_get_path;
use apollo_hyper_libretro_core_handy::get_path as handy_get_path;
use apollo_hyper_libretro_core_melonds::get_path as melonds_get_path;
use apollo_hyper_libretro_core_mgba::get_path as mgba_get_path;
use apollo_hyper_libretro_core_mupen64plus_next::get_path as mupen64plus_next_get_path;
use apollo_hyper_libretro_core_neocd::get_path as neocd_get_path;
use apollo_hyper_libretro_core_nestopia_ue::get_path as nestopia_ue_get_path;
use apollo_hyper_libretro_core_o2em::get_path as o2em_get_path;
use apollo_hyper_libretro_core_opera::get_path as opera_get_path;
use apollo_hyper_libretro_core_parallel_64::get_path as parallel_64_get_path;
use apollo_hyper_libretro_core_prosystem::get_path as prosystem_get_path;
use apollo_hyper_libretro_core_snes9x::get_path as snes9x_get_path;
use apollo_hyper_libretro_core_stella_2014::get_path as stella_2014_get_path;
use apollo_hyper_libretro_core_vecx::get_path as vecx_get_path;
use apollo_hyper_libretro_core_virtual_jaguar::get_path as virtual_jaguar_get_path;
use apollo_hyper_libretro_core_yabause::get_path as yabause_get_path;

pub fn get_all() -> Vec<&'static str> {
    const FUNCTIONS: [fn() -> Option<&'static str>; 24] = [
        a5200_get_path,
        beetle_ngp_get_path,
        beetle_psx_get_path,
        beetle_vb_get_path,
        beetle_wswan_get_path,
        freechaf_get_path,
        freeintv_get_path,
        gearcoleco_get_path,
        genesis_plus_gx_get_path,
        mgba_get_path,
        handy_get_path,
        melonds_get_path,
        mupen64plus_next_get_path,
        neocd_get_path,
        nestopia_ue_get_path,
        o2em_get_path,
        opera_get_path,
        parallel_64_get_path,
        prosystem_get_path,
        snes9x_get_path,
        stella_2014_get_path,
        vecx_get_path,
        virtual_jaguar_get_path,
        yabause_get_path,
    ];

    FUNCTIONS.into_iter().filter_map(|x| x()).collect()
}

#[test]
fn it_works() {
    dbg!(get_all());
}
