use crate::dir::get_out_dir;
use std::process::{Command, Output};

pub fn run_build_cmd(command: &mut Command) -> Option<Output> {
    println!("{}", get_out_dir().display());
    let output = command
        .current_dir(get_out_dir())
        .spawn()
        .expect("Make could not be invoked!");
    let out = output
        .wait_with_output()
        .expect("The core did not build successfully!");
    if !out.status.success() {
        println!("cargo:warning=Command did not exit successfuly!");
        println!("cargo:rustc-env=COREPATH=none");
        None
    } else {
        Some(out)
    }
}
