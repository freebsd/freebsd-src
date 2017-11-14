# $FreeBSD$

.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=        -march=i386
CFLAGS+=	-mno-aes
.endif

# Options used when building app-specific efi components
# See conf/kern.mk for the correct set of these
CFLAGS+=	-ffreestanding -Wformat ${CFLAGS_NO_SIMD}
LDFLAGS+=	-nostdlib

.if ${MACHINE_CPUARCH} != "aarch64"
CFLAGS+=	-msoft-float
.endif

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-fshort-wchar
CFLAGS+=	-mno-red-zone
CFLAGS+=	-mno-aes
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-fshort-wchar
CFLAGS+=	-fPIC
.endif

.if ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-fPIC
.endif

.include "../Makefile.inc"
