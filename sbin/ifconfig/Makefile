#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ifconfig/Makefile,v 1.14.2.2 2001/04/17 15:21:02 luigi Exp $

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA
CFLAGS+=-DINET6
.if !defined(RELEASE_CRUNCH)

#comment out to exclude SIOC[GS]ETVLAN support
SRCS+=	ifvlan.c
CFLAGS+=-DUSE_VLANS
.endif

MAN8=	ifconfig.8
DPADD=	${LIBIPX}
LDADD=	-lipx
COPTS=	-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs -I..

.include <bsd.prog.mk>
