# $FreeBSD: src/sbin/natd/Makefile,v 1.9.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
