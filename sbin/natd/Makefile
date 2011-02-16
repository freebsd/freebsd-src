# $FreeBSD: src/sbin/natd/Makefile,v 1.9.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
