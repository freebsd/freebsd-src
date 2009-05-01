# $FreeBSD: src/sys/modules/wlan_ccmp/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../net80211
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c

.include <bsd.kmod.mk>
