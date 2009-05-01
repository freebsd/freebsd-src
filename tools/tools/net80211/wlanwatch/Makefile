# $FreeBSD: src/tools/tools/net80211/wlanwatch/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	wlanwatch
BINDIR=	/usr/local/bin
NO_MAN=

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
