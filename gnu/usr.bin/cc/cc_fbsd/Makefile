# $FreeBSD$

LIBCDIR=	${.CURDIR}/../../../../lib/libc

.PATH: ${LIBCDIR}/gen ${LIBCDIR}/stdio

LIB=		cc_fbsd
NOPROFILE=	YES
NOPIC=		YES

SRCS=	getobjformat.c mktemp.c
	
install:
	@true

.include <bsd.lib.mk>
