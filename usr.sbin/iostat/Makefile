#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.12 2003/04/04 17:49:14 obrien Exp $

MAINTAINER=	ken@FreeBSD.ORG

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBKVM} ${LIBDEVSTAT} ${LIBM}
LDADD=	-lkvm -ldevstat -lm

.include <bsd.prog.mk>
