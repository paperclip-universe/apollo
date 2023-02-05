use std::{env::var, path::{Path, PathBuf}};

pub fn get_out_dir() -> PathBuf {
    Path::new(&var("OUT_DIR").unwrap()).join(var("CARGO_PKG_NAME").unwrap())
}
