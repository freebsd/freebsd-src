# $FreeBSD: src/usr.sbin/ndc/Makefile,v 1.7 1999/11/30 06:18:09 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/ndc
.PATH:	${BIND_DIR}/doc/man

PROG=	ndc
SRCS=	pathnames.h \
	ndc.c 
MAN8=	ndc.8

.include <bsd.prog.mk>
