# $FreeBSD: src/usr.sbin/ndc/Makefile,v 1.5.2.2 1999/12/13 15:08:28 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/ndc
.PATH:	${BIND_DIR}/doc/man

PROG=	ndc
SRCS=	pathnames.h \
	ndc.c 
MAN8=	ndc.8

.include <bsd.prog.mk>
