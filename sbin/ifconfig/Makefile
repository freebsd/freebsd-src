#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA
CFLAGS+=-DINET6

#comment out to exclude SIOC[GS]ETVLAN support
SRCS+=	ifvlan.c
CFLAGS+=-DUSE_VLANS

#comment out to exclude SIOC[GS]IEEE80211 support
SRCS+=	ifieee80211.c
CFLAGS+=-DUSE_IEEE80211

MAN=	ifconfig.8

.if defined(RELEASE_CRUNCH)
CFLAGS+=-DNO_IPX
.else
DPADD=	${LIBIPX}
LDADD=	-lipx
.endif

CFLAGS+=-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs -I..
WARNS=	0

.include <bsd.prog.mk>
