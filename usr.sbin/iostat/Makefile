#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.13 2003/06/14 05:28:00 ken Exp $

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBKVM} ${LIBDEVSTAT} ${LIBM}
LDADD=	-lkvm -ldevstat -lm

.include <bsd.prog.mk>
