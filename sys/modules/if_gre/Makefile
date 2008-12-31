# $FreeBSD: src/sys/modules/if_gre/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gre
SRCS=	if_gre.c ip_gre.c opt_inet.h opt_inet6.h opt_atalk.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

opt_atalk.h:
	echo "#define NETATALK 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
