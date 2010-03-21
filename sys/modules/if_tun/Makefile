# $FreeBSD: src/sys/modules/if_tun/Makefile,v 1.29.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_tun
SRCS=	if_tun.c opt_atalk.h opt_inet.h opt_inet6.h opt_ipx.h opt_mac.h

.if !defined(KERNBUILDDIR)
opt_atalk.h:
	echo "#define NETATALK 1" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
