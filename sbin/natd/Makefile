# $FreeBSD$

MAINTAINER	= ru@FreeBSD.org
MAINTAINER     += ari@suutari.iki.fi
PROG		= natd
SRCS		= natd.c icmp.c
CFLAGS	       += -Wall
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN8		= natd.8

.include <bsd.prog.mk>
