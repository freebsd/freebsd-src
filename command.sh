/usr/local/llvm17/bin/clang -fplugin=/mnt/openbsd_list_macro_printer/build/lib/Release/libopenbsd_list_macro_printer.so -fsyntax-only -fno-builtin \
-target aarch64-unknown-freebsd15.0 --sysroot=/usr/obj/usr/src/arm64.aarch64/tmp \
-B/usr/obj/usr/src/arm64.aarch64/tmp/usr/bin -I/usr/src/sys -I/usr/src/sys/contrib \
-I/usr/src/include -I/usr/src/contrib/llvm-project/clang/lib/Headers \
-I/usr/src/sys/contrib/ck -I/usr/src/sys/contrib/ck/src -I/usr/src/sys/contrib/ck/include -I/usr/src/sys/contrib/libfdt \
-I/usr/obj/usr/src/arm64.aarch64/sys/GENERIC -D_KERNEL -I/usr/src/sys/contrib/xz-embedded/linux/include/linux \
-I/usr/src/sys/contrib/xz-embedded/freebsd -I/usr/src/sys/contrib/zstd/lib/common \
-I/usr/src/sys/contrib/openzfs/module/zstd/include -I/usr/local/llvm17/lib/clang/17/include \
-I/usr/src/sys/crypto/blake2 -I/usr/src/sys/contrib/libsodium/src/libsodium/include \
-include /usr/obj/usr/src/arm64.aarch64/sys/GENERIC/opt_global.h -nostdinc \
/usr/src/sys/kern/kern_proc.c