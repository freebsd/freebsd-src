# $FreeBSD: src/sys/modules/if_vlan/Makefile,v 1.2 2002/01/11 15:48:48 ru Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_vlan
SRCS=	if_vlan.c opt_inet.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
