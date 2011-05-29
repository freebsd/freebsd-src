# $FreeBSD$

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc
LDFLAGS+=	-m elf32ppc
.endif

.include "../Makefile.inc"
