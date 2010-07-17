# $FreeBSD: src/sys/modules/netgraph/gif/Makefile,v 1.5.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=	ng_gif
SRCS= 	ng_gif.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
