# $FreeBSD$

BINDIR?=	/boot

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=        -march=i386
.endif

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=        -m32
ACFLAGS+=       -m32
LDFLAGS+=       -m elf_i386_fbsd
AFLAGS+=        --32
.endif

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
