fn main() {
    let target = std::env::var("TARGET").expect("TARGET is set by Cargo");
    let mut build = cc::Build::new();
    build
        .file("c/ame_compat.c")
        .include("include")
        .warnings(true);
    if target.contains("x86_64") && !target.contains("windows") {
        build.file("asm/ame_compat_x86_64.S");
    }
    build.compile("ame_compat");

    println!("cargo:rerun-if-changed=c/ame_compat.c");
    println!("cargo:rerun-if-changed=asm/ame_compat_x86_64.S");
    println!("cargo:rerun-if-changed=include/ame_compat.h");
    println!("cargo:rerun-if-changed=cpp/ame_compat.hpp");
}
