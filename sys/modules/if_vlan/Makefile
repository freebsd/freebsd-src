# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	if_vlan
SRCS=	if_vlan.c opt_inet.h
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
