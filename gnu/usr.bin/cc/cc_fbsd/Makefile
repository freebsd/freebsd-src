# $FreeBSD$

LIBCDIR=	${.CURDIR}/../../../../lib/libc

.PATH: ${LIBCDIR}/gen ${LIBCDIR}/stdio

LIB=		cc_fbsd
INTERNALLIB=	YES

SRCS=	getobjformat.c mktemp.c

CFLAGS+= -D_open=open

.include <bsd.lib.mk>
