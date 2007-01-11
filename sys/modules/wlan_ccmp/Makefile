# $FreeBSD: src/sys/modules/wlan_ccmp/Makefile,v 1.3 2004/12/16 01:10:48 sam Exp $

.PATH: ${.CURDIR}/../../net80211
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c

.include <bsd.kmod.mk>
