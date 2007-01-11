# $FreeBSD: src/sbin/natd/Makefile,v 1.9 2004/02/23 20:13:55 johan Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
