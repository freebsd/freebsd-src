# $FreeBSD: src/sys/modules/if_bridge/Makefile,v 1.2.4.1 2005/10/03 21:55:15 thompsa Exp $

.PATH:	${.CURDIR}/../../net
KMOD=	if_bridge
SRCS=	if_bridge.c bridgestp.c opt_inet.h opt_inet6.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	
.if !defined(NOINET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
