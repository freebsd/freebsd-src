# $FreeBSD: src/usr.sbin/ndc/Makefile,v 1.11 2003/04/04 17:49:15 obrien Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH: ${BIND_DIR}/bin/ndc ${BIND_DIR}/doc/man

PROG=	ndc
MAN=	ndc.8
SRCS=	pathnames.h \
	ndc.c 

.include <bsd.prog.mk>
