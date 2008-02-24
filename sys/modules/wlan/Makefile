# $FreeBSD: src/sys/modules/wlan/Makefile,v 1.8 2007/06/11 07:26:35 thompsa Exp $

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan
SRCS=	ieee80211.c ieee80211_crypto.c ieee80211_crypto_none.c \
	ieee80211_freebsd.c ieee80211_input.c ieee80211_ioctl.c \
	ieee80211_node.c ieee80211_output.c ieee80211_power.c \
	ieee80211_proto.c ieee80211_scan.c ieee80211_regdomain.c \
	ieee80211_ht.c
SRCS+=	bus_if.h device_if.h opt_compat.h opt_inet.h opt_ipx.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h
.endif

.include <bsd.kmod.mk>
