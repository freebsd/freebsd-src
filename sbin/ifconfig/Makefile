#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA
.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DINET6

#comment out to exclude SIOC[GS]ETVLAN support
SRCS+=	ifvlan.c
CFLAGS+=-DUSE_VLANS
.endif

MAN=	ifconfig.8
DPADD=	${LIBIPX}
LDADD=	-lipx
COPTS=	-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs -I..

.include <bsd.prog.mk>
