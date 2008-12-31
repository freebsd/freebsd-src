# $FreeBSD: src/sys/modules/wlan_ccmp/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net80211
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c

.include <bsd.kmod.mk>
