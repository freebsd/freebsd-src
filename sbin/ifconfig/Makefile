#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.10 1997/05/04 06:27:45 peter Exp $

PROG=	ifconfig
SRCS=	ifconfig.c

#comment out to exclude SIOC[GS]IFMEDIA support
SRCS+=	ifmedia.c
CFLAGS+=-DUSE_IF_MEDIA

#comment out to exclude SIOC[GS]ETVLAN support
SRCS+=	ifvlan.c
CFLAGS+=-DUSE_VLANS

MAN8=	ifconfig.8
DPADD=	${LIBIPX}
LDADD=	-lipx
COPTS=	-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs -I..

.include <bsd.prog.mk>
