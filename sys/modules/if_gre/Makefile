# $FreeBSD$

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet 

KMOD=	if_gre
SRCS=	if_gre.c ip_gre.c opt_inet.h opt_ns.h opt_atalk.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_ns.h:
	echo "#define NS 1" > ${.TARGET}

opt_atalk.h:
	echo "#define NETATALK 1" > ${.TARGET}

.include <bsd.kmod.mk>
