# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	arcnet
SRCS=	if_arcsubr.c
SRCS+=	opt_inet.h opt_inet6.h

opt_inet.h opt_inet6.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
