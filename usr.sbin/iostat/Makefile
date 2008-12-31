#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.14.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBM}
LDADD=	-ldevstat -lkvm -lm

.include <bsd.prog.mk>
