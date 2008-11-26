# $FreeBSD: src/usr.sbin/kldxref/Makefile,v 1.7.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c ef.c ef_obj.c

WARNS?=	2

.if exists(ef_${MACHINE_ARCH}.c)
SRCS+=	ef_${MACHINE_ARCH}.c
.else
SRCS+=	ef_nop.c
.endif

.include <bsd.prog.mk>
