# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	wlan
SRCS=	if_ieee80211subr.c
SRCS+=	bus_if.h device_if.h opt_inet.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
