fn main() {
    cc::Build::new()
        .file("c/ame_compat.c")
        .include("include")
        .warnings(true)
        .compile("ame_compat");

    println!("cargo:rerun-if-changed=c/ame_compat.c");
    println!("cargo:rerun-if-changed=include/ame_compat.h");
    println!("cargo:rerun-if-changed=cpp/ame_compat.hpp");
}
