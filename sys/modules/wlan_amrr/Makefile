# $FreeBSD: src/sys/modules/wlan_amrr/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan_amrr
SRCS=	ieee80211_amrr.c
SRCS+=	opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
