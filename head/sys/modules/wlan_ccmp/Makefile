# $FreeBSD$

.PATH: ${.CURDIR}/../../net80211
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c
SRCS+=	opt_wlan.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
