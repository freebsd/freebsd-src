# $FreeBSD: src/usr.sbin/ndc/Makefile,v 1.10 2001/07/20 06:20:03 obrien Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH: ${BIND_DIR}/bin/ndc
.PATH: ${BIND_DIR}/doc/man

PROG=	ndc
MAN=	ndc.8
SRCS=	pathnames.h \
	ndc.c 

.include <bsd.prog.mk>
