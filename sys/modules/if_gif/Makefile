# $FreeBSD$

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c in6_gif.c opt_inet.h opt_inet6.h opt_mrouting.h
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}

.include <bsd.kmod.mk>
