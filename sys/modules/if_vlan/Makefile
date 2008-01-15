# $FreeBSD: src/sys/modules/if_vlan/Makefile,v 1.2.14.1 2006/02/10 00:38:03 yar Exp $

.PATH: ${.CURDIR}/../../net

KMOD=	if_vlan
SRCS=	if_vlan.c opt_inet.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
