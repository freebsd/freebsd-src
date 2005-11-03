# $FreeBSD: src/sys/modules/if_bridge/Makefile,v 1.2 2005/06/05 05:28:41 ru Exp $

.PATH:	${.CURDIR}/../../net
KMOD=	if_bridge
SRCS=	if_bridge.c bridgestp.c opt_inet.h opt_inet6.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	
.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
