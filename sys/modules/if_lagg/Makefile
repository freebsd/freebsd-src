# $FreeBSD: src/sys/modules/if_lagg/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../net
KMOD=	if_lagg
SRCS=	if_lagg.c ieee8023ad_lacp.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
