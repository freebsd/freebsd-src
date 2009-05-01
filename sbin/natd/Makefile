# $FreeBSD: src/sbin/natd/Makefile,v 1.9.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
