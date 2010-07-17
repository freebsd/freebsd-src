# $FreeBSD: src/sbin/natd/Makefile,v 1.9.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
