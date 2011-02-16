# $FreeBSD: src/usr.sbin/nghook/Makefile,v 1.8.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $
# $Whistle: Makefile,v 1.4 1999/01/16 04:44:33 archie Exp $

PROG=	nghook
MAN=	nghook.8
SRCS=	main.c
WARNS?=	6

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.prog.mk>
