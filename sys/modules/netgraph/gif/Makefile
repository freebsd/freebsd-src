# $FreeBSD: src/sys/modules/netgraph/gif/Makefile,v 1.2 2001/09/27 03:14:16 brooks Exp $

KMOD=	ng_gif
SRCS= 	ng_gif.c opt_inet.h opt_inet6.h

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

.include <bsd.kmod.mk>
