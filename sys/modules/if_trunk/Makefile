# $FreeBSD$

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../net
KMOD=	if_trunk
SRCS=	if_trunk.c ieee8023ad_lacp.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
