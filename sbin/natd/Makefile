# $FreeBSD: src/sbin/natd/Makefile,v 1.4.2.1 2001/04/25 10:58:41 ru Exp $

MAINTAINER	= ru@FreeBSD.org
MAINTAINER     += ari@suutari.iki.fi
PROG		= natd
SRCS		= natd.c icmp.c
CFLAGS	       += -Wall
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
