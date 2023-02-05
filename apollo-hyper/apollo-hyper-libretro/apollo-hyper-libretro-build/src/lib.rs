mod build;
mod clone;
mod dir;

use build::run_build_cmd;
use clone::clone_repo;
use dir::get_out_dir;
use glob::glob;
use owo_colors::OwoColorize;
use std::{
    env, fs,
    io::Write,
    path::Path,
    process::{exit, Command, Stdio},
};

fn is_program_in_path(program: &str) -> bool {
    if let Ok(path) = env::var("PATH") {
        for p in path.split(':') {
            let p_str = format!("{p}/{program}");
            if fs::metadata(p_str).is_ok() {
                return true;
            }
        }
    }
    false
}

fn assert_clis(programs: Vec<&str>) {
    for program in programs {
        if !is_program_in_path(program) {
            println!("{}: {program} is not installed!", "error".red().bold());
            exit(1);
        }
    }
}

pub fn build(dir: &str, patch: Option<&str>, mf: Option<&str>, repo: &str) {
    assert_clis(vec!["git", "patch", "make", "nasm"]);
    let target_os = env::var("TARGET").unwrap();

    clone_repo(repo).unwrap();

    glob(
        get_out_dir()
            .join("/*/build/apollo_libretro*")
            .to_str()
            .unwrap(),
    )
    .unwrap()
    .filter_map(Result::ok)
    .map(fs::remove_file)
    .for_each(drop);

    if !get_out_dir().join("build").exists() {
        fs::create_dir(get_out_dir().join("build")).unwrap();
    }

    if target_os.starts_with("wasm") {
        if let Some(patch_file) = patch {
            let mut output = Command::new("patch")
                .args(["-r", "-", "--forward"])
                .current_dir(format!("./{dir}"))
                .stdin(Stdio::piped())
                .spawn()
                .expect("Git patch could not be invoked!");

            if let Some(mut stdin) = output.stdin.take() {
                stdin
                    .write_all(
                        fs::read_to_string(format!("./{patch_file}"))
                            .unwrap()
                            .as_bytes(),
                    )
                    .unwrap();
                drop(stdin)
            }

            output
                .wait_with_output()
                .expect("The core did not patch successfully!");
        }

        run_build_cmd(Command::new("emmake").args([
            "make",
            "-j",
            "-f",
            match mf {
                None => "Makefile",
                Some(x) => x,
            },
            "platform=emscripten",
            "TARGET_NAME=build/apollo",
        ]));
    } else {
        run_build_cmd(Command::new("make").args([
            "-j",
            "-f",
            match mf {
                Some(x) => x,
                None => "Makefile",
            },
            "TARGET_NAME=build/apollo",
        ]));
    }
    println!(
        "cargo:rustc-env=COREPATH={}",
        fs::canonicalize(
            glob(
                get_out_dir()
                    .join("build/apollo_libretro*")
                    .to_str()
                    .unwrap()
            )
            .unwrap()
            .filter_map(Result::ok)
            .next()
            .unwrap()
        )
        .expect("oops")
        .to_string_lossy()
    )
}
