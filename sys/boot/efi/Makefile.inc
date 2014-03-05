# $FreeBSD$

BINDIR?=	/boot

.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=        -march=i386
.endif

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
