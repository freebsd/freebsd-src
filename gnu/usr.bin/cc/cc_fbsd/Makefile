# $FreeBSD: src/gnu/usr.bin/cc/cc_fbsd/Makefile,v 1.3.2.1 2002/07/19 18:46:23 ru Exp $

LIBCDIR=	${.CURDIR}/../../../../lib/libc

.PATH: ${LIBCDIR}/gen ${LIBCDIR}/stdio

LIB=		cc_fbsd
INTERNALLIB=	YES

SRCS=	getobjformat.c mktemp.c

CFLAGS+= -D_open=open

.include <bsd.lib.mk>
