# $FreeBSD: src/sbin/ipfw/Makefile,v 1.21.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c altq.c
WARNS?=	2
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	ipfw.8

.include <bsd.prog.mk>
