# $FreeBSD$

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/ndc
.PATH:	${BIND_DIR}/doc/man

PROG=	ndc
SRCS=	pathnames.h \
	ndc.c 
MAN=	ndc.8

.include <bsd.prog.mk>
