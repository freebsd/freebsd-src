# $FreeBSD: src/sys/modules/wlan_wep/Makefile,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan_wep
SRCS=	ieee80211_crypto_wep.c
SRCS+=	opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
