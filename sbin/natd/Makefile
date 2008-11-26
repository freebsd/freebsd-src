# $FreeBSD: src/sbin/natd/Makefile,v 1.9.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
