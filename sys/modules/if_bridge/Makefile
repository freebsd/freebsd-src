# $FreeBSD: src/sys/modules/if_bridge/Makefile,v 1.2.2.2.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../net
KMOD=	if_bridge
SRCS=	if_bridge.c bridgestp.c opt_inet.h opt_inet6.h opt_carp.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	
.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
