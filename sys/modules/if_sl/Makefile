# $FreeBSD: src/sys/modules/if_sl/Makefile,v 1.19 2002/01/11 15:48:48 ru Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
