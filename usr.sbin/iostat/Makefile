#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.11 2001/08/23 03:19:54 ken Exp $

MAINTAINER=	ken@FreeBSD.ORG

PROG=	iostat
DPADD=	${LIBKVM} ${LIBDEVSTAT} ${LIBM}
LDADD=	-lkvm -ldevstat -lm
MAN=	iostat.8

.include <bsd.prog.mk>
