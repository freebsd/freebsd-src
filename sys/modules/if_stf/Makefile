# $FreeBSD: src/sys/modules/if_stf/Makefile,v 1.4.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_stf
SRCS=	if_stf.c opt_inet.h opt_inet6.h opt_mac.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
