# $FreeBSD: src/sbin/ipfw/Makefile,v 1.21.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c altq.c
WARNS?=	2
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	ipfw.8

.include <bsd.prog.mk>
