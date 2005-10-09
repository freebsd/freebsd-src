# $FreeBSD$

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan
SRCS=	ieee80211.c ieee80211_crypto.c ieee80211_crypto_none.c \
	ieee80211_freebsd.c ieee80211_input.c ieee80211_ioctl.c \
	ieee80211_node.c ieee80211_output.c ieee80211_proto.c
SRCS+=	bus_if.h device_if.h opt_inet.h opt_ipx.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

.include <bsd.kmod.mk>
