# $FreeBSD: src/sbin/natd/Makefile,v 1.2.2.2 1999/10/28 18:55:34 ru Exp $

MAINTAINER	= ru@FreeBSD.org
MAINTAINER     += ari@suutari.iki.fi
PROG		= natd
SRCS		= natd.c icmp.c
CFLAGS	       += -Wall
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN8		= natd.8

.include <bsd.prog.mk>
