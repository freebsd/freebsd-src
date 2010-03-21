# $FreeBSD: src/sbin/natd/Makefile,v 1.9.32.1 2010/02/10 00:26:20 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
