# $FreeBSD: src/sys/modules/if_vlan/Makefile,v 1.2.14.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_vlan
SRCS=	if_vlan.c opt_inet.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
