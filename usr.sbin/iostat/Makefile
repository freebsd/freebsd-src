#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.14 2004/02/04 10:20:43 ru Exp $

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBM}
LDADD=	-ldevstat -lkvm -lm

.include <bsd.prog.mk>
