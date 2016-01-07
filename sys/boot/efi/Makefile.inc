# $FreeBSD$

BINDIR?=	/boot

.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=        -march=i386
.endif

# Options used when building app-specific efi components
# See conf/kern.mk for the correct set of these
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
CFLAGS+=	-mno-red-zone
CFLAGS+=	-mno-mmx -mno-sse -mno-aes -mno-avx -msoft-float
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
