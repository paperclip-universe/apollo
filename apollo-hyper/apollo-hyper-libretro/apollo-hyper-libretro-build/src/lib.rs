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

fn assert_cli(program: &str) {
    if !is_program_in_path(program) {
        println!("{}: {program} is not installed!", "error".red().bold());
        exit(1);
    }
}

pub fn build(dir: &str, patch: Option<&str>, mf: Option<&str>) {
    let target_os = env::var("TARGET").unwrap();
    const CI_MAKE_J: &str = "-j";

    glob("./*/build/apollo_libretro*")
        .unwrap()
        .filter_map(Result::ok)
        .map(fs::remove_file)
        .for_each(drop);

    if !Path::new(&format!("./{dir}/build")).exists() {
        fs::create_dir(format!("./{dir}/build")).unwrap();
    }

    if target_os.starts_with("wasm") {
        assert_cli("emmake");

        if let Some(patch_file) = patch {
            assert_cli("git");
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

            let _ = output
                .wait_with_output()
                .expect("The core did not patch successfully!");
        }

        let output = Command::new("emmake")
            .args([
                "make",
                "-f",
                match mf {
                    None => "Makefile",
                    Some(x) => x,
                },
                "platform=emscripten",
                "TARGET_NAME=build/apollo",
            ])
            .current_dir(fs::canonicalize(format!("./{dir}")).unwrap())
            .spawn()
            .expect("Make could not be invoked!");
        let out = output
            .wait_with_output()
            .expect("The core did not build successfully!");
        if !out.status.success() {
            println!("cargo:warning=Command did not exit successfuly!");
            println!("cargo:warning=Command: ");
            println!(
                "{}",
                [
                    "emmake",
                    "-j",
                    "-f",
                    match mf {
                        Some(x) => x,
                        None => "Makefile",
                    },
                    "TARGET_NAME=build/apollo",
                ]
                .into_iter()
                .map(|x| format!("cargo:warning={x}"))
                .collect::<String>()
            );
            println!("cargo:rustc-env=COREPATH=none");
            return;
        }
    } else {
        assert_cli("make");

        let output = Command::new("make")
            .args([
                "-j",
                "-f",
                match mf {
                    Some(x) => x,
                    None => "Makefile",
                },
                "TARGET_NAME=build/apollo",
            ])
            .current_dir(format!("./{dir}"))
            .spawn()
            .expect("Make could not be invoked!");
        let out = output
            .wait_with_output()
            .expect("The child could not be invoked!");
        if !out.status.success() {
            println!("cargo:warning=Command did not exit successfuly!");
            println!("cargo:warning=Command: ");
            println!(
                "{}",
                [
                    "make",
                    "-j",
                    "-f",
                    match mf {
                        Some(x) => x,
                        None => "Makefile",
                    },
                    "TARGET_NAME=build/apollo",
                ]
                .into_iter()
                .map(|x| format!("cargo:warning={x}"))
                .collect::<String>()
            );
            println!("cargo:rustc-env=COREPATH=none");
            return;
        }
    }
    println!(
        "cargo:rustc-env=COREPATH={}",
        fs::canonicalize(
            glob(&format!("./{dir}/build/apollo_libretro*"))
                .unwrap()
                .filter_map(Result::ok)
                .collect::<Vec<_>>()
                .first()
                .unwrap()
        )
        .expect("oops")
        .to_string_lossy()
    )
}
