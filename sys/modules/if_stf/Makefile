# $FreeBSD: src/sys/modules/if_stf/Makefile,v 1.1.2.1 2001/07/24 19:10:17 brooks Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_stf
SRCS=	if_stf.c opt_inet.h opt_inet6.h
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

.include <bsd.kmod.mk>
