# $FreeBSD: src/sys/modules/netgraph/ipfw/Makefile,v 1.1.22.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

KMOD=	ng_ipfw
SRCS= 	ng_ipfw.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)

.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
