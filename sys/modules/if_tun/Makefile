# $FreeBSD: src/sys/modules/if_tun/Makefile,v 1.28 2004/12/29 08:45:48 imp Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_tun
SRCS=	if_tun.c opt_atalk.h opt_inet.h opt_inet6.h opt_ipx.h opt_mac.h

opt_atalk.h:
	echo "#define NETATALK 1" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}

.include <bsd.kmod.mk>
