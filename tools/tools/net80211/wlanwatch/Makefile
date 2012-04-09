# $FreeBSD: src/tools/tools/net80211/wlanwatch/Makefile,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	wlanwatch
BINDIR=	/usr/local/bin
NO_MAN=

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
