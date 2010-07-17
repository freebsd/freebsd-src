# $FreeBSD: src/sbin/ipfw/Makefile,v 1.21.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c altq.c
WARNS?=	2
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	ipfw.8

.include <bsd.prog.mk>
