# $FreeBSD: src/sbin/natd/Makefile,v 1.4.2.2 2003/06/27 09:31:26 ru Exp $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
