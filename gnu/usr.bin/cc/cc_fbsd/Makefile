# $FreeBSD: src/gnu/usr.bin/cc/cc_fbsd/Makefile,v 1.3 2000/01/28 08:49:23 jasone Exp $

LIBCDIR=	${.CURDIR}/../../../../lib/libc

.PATH: ${LIBCDIR}/gen ${LIBCDIR}/stdio

LIB=		cc_fbsd
NOPROFILE=	YES
NOPIC=		YES

SRCS=	getobjformat.c mktemp.c

CFLAGS+= -D_open=open

install:
	@true

.include <bsd.lib.mk>
