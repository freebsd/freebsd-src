# $FreeBSD: src/sbin/natd/Makefile,v 1.9.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
