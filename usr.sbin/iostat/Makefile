#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAINTAINER=	ken@FreeBSD.ORG

PROG=	iostat
DPADD=	${LIBKVM} ${LIBDEVSTAT} ${LIBM}
LDADD=	-lkvm -ldevstat -lm
MAN=	iostat.8

.include <bsd.prog.mk>
