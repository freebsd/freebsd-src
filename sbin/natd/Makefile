# $FreeBSD$

MAINTAINER	= ru@FreeBSD.org
MAINTAINER     += ari@suutari.iki.fi
PROG		= natd
SRCS		= natd.c icmp.c
WARNS=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
