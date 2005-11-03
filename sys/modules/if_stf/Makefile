# $FreeBSD: src/sys/modules/if_stf/Makefile,v 1.3 2002/10/20 22:57:22 rwatson Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_stf
SRCS=	if_stf.c opt_inet.h opt_inet6.h opt_mac.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

.include <bsd.kmod.mk>
