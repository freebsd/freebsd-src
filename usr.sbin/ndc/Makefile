# $FreeBSD$

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH: ${BIND_DIR}/bin/ndc ${BIND_DIR}/doc/man

PROG=	ndc
MAN=	ndc.8
SRCS=	pathnames.h \
	ndc.c 

.include <bsd.prog.mk>
