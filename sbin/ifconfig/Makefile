#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ifconfig/Makefile,v 1.10.2.1 1999/08/29 15:13:39 peter Exp $

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA

MAN8=	ifconfig.8
DPADD=	${LIBIPX}
LDADD=	-lipx
COPTS=	-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs

.include <bsd.prog.mk>
