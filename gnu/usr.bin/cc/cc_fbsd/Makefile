# $FreeBSD$

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
