# $FreeBSD$

PROG=	kldxref
SRCS=	kldxref.c ef.c
WARNS?=	2
MAN=	kldxref.8

.if exists(ef_${MACHINE_ARCH}.c)
SRCS+=	ef_${MACHINE_ARCH}.c
.else
SRCS+=	ef_nop.c
.endif

.include <bsd.prog.mk>
