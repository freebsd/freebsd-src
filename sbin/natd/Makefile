# $FreeBSD: src/sbin/natd/Makefile,v 1.8 2002/04/12 19:11:09 ru Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
