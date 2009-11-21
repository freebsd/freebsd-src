# $FreeBSD: src/usr.sbin/nghook/Makefile,v 1.8.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $
# $Whistle: Makefile,v 1.4 1999/01/16 04:44:33 archie Exp $

PROG=	nghook
MAN=	nghook.8
SRCS=	main.c
WARNS?=	6

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.prog.mk>
