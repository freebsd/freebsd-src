#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAINTAINER=	ken@FreeBSD.ORG

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat

.include <bsd.prog.mk>
