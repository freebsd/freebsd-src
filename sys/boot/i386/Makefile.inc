# Common defines for all of /sys/boot/i386/
#
# $FreeBSD$

LOADER_ADDRESS?=	0x200000
CFLAGS+=		-ffreestanding -mpreferred-stack-boundary=2

.if ${MACHINE_ARCH} == "amd64"
.MAKEFLAGS:  MACHINE_ARCH=i386 MACHINE=i386 REALLY_AMD64=true
.endif

.if defined(REALLY_AMD64) && !defined(__been_to_Makefile_inc)
__been_to_Makefile_inc=	1
CFLAGS+=		-m32
LDFLAGS+=		-m elf_i386_fbsd
AFLAGS+=		--32
.endif
