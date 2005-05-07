# $FreeBSD: src/sys/modules/if_disc/Makefile,v 1.17 2002/01/11 15:48:40 ru Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_disc
SRCS=	if_disc.c opt_inet.h opt_inet6.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

#opt_inet6.h:
#	echo "#define INET6 1" > ${.TARGET}

.include <bsd.kmod.mk>
