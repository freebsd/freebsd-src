# $FreeBSD$

.PATH:	${.CURDIR}/../../net
KMOD=	if_bridge
SRCS=	if_bridge.c bridgestp.c opt_inet.h opt_inet6.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	
opt_inet6.h:
.if defined(NOINET6)
	echo > ${.TARGET}
.else
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
