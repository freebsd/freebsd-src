# $FreeBSD: src/sys/modules/wlan_amrr/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan_amrr
SRCS=	ieee80211_amrr.c
SRCS+=	opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
