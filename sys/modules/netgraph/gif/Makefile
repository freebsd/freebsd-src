# $FreeBSD: src/sys/modules/netgraph/gif/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

KMOD=	ng_gif
SRCS= 	ng_gif.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
