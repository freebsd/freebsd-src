# $FreeBSD: src/tools/tools/net80211/wlanwatch/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	wlanwatch
BINDIR=	/usr/local/bin
NO_MAN=

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
