# $FreeBSD: src/sys/modules/if_lagg/Makefile,v 1.1.2.1 2007/05/17 01:45:15 thompsa Exp $

.PATH:	${.CURDIR}/../../net
KMOD=	if_lagg
SRCS=	if_lagg.c ieee8023ad_lacp.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
