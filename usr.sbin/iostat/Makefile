#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBKVM} ${LIBDEVSTAT} ${LIBM}
LDADD=	-lkvm -ldevstat -lm

.include <bsd.prog.mk>
