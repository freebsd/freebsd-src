# $FreeBSD$

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan_xauth
SRCS=	ieee80211_xauth.c
SRCS+=	opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > opt_wlan.h
.endif

.include <bsd.kmod.mk>
