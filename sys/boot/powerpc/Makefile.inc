# $FreeBSD$

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc
.endif

.include "../Makefile.inc"
