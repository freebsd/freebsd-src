# $FreeBSD: src/usr.sbin/ndc/Makefile,v 1.7.2.1 2001/04/25 12:10:19 ru Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/ndc
.PATH:	${BIND_DIR}/doc/man

PROG=	ndc
SRCS=	pathnames.h \
	ndc.c 
MAN=	ndc.8

.include <bsd.prog.mk>
