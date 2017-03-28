#!/bin/sh -e

CHERI_ROOT="${HOME}/cheri"
CHERI_OUTPUT="${CHERI_ROOT}/output"
CHERI_SDK256="${CHERI_OUTPUT}/sdk256"
CHERI_FREEBSD="${CHERI_OUTPUT}/freebsd-mips"
ST_SRC="${HOME}/cheri/cheribsd/tools/tools/syscall_timing"
ST_INSTALL="${HOME}/syscall_timing"

CFLAGS_COMMON="-pipe -O2 -msoft-float -ggdb -static -integrated-as -Wcheri-capability-misuse -Werror=implicit-function-declaration -Werror=format -Werror=undefined-internal"

# Don't add -fstack-protector-strong; it breaks CHERI binaries.
SSP_CFLAGS="" export SSP_CFLAGS

CC="${CHERI_SDK256}/bin/clang" export CC
CFLAGS="${CFLAGS_COMMON} --sysroot=${CHERI_SDK256}/sysroot -B${CHERI_SDK256}/bin -target cheri-unknown-freebsd -mabi=purecap" export CFLAGS
mkdir -p "${ST_INSTALL}/cheri"
cd "${ST_SRC}"
make clean all
cp "${ST_SRC}/syscall_timing" "${ST_INSTALL}/cheri/"

CC="${CHERI_SDK256}/bin/clang" export CC
CFLAGS="${CFLAGS_COMMON} --sysroot=${CHERI_FREEBSD} -B${CHERI_SDK256}/bin -target mips64-unknown-freebsd -mabi=n64" export CFLAGS
mkdir -p "${ST_INSTALL}/mips"
cd "${ST_SRC}"
make clean all
cp "${ST_SRC}/syscall_timing" "${ST_INSTALL}/mips/"

