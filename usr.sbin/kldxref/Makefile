# $FreeBSD: src/usr.sbin/kldxref/Makefile,v 1.6 2003/04/04 17:49:14 obrien Exp $

PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c ef.c

WARNS?=	2

.if exists(ef_${MACHINE_ARCH}.c)
SRCS+=	ef_${MACHINE_ARCH}.c
.else
SRCS+=	ef_nop.c
.endif

.include <bsd.prog.mk>
