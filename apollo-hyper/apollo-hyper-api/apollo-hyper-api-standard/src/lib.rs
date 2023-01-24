#![allow(incomplete_features)]
#![feature(return_position_impl_trait_in_trait)]
use std::path::Path;

pub trait ApolloMultiEmulator {
    fn create_emulator(core_path: &Path, rom_path: &Path) -> impl ApolloEmulator;
}

pub trait ApolloEmulator {
    fn boot(&self, rom_path: &Path);
}
