# $FreeBSD$

.PATH:  ${.CURDIR}/../../net
KMOD=	if_sl
SRCS=	if_sl.c slcompress.c opt_inet.h opt_slip.h
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
