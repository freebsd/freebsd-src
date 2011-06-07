# $FreeBSD$

PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c ef.c ef_obj.c

WARNS?=	2
CFLAGS+=-fno-strict-aliasing

.if exists(ef_${MACHINE_CPUARCH}.c) && ${MACHINE_ARCH} != "powerpc64"
SRCS+=	ef_${MACHINE_CPUARCH}.c
.else
SRCS+=	ef_nop.c
.endif

.include <bsd.prog.mk>
