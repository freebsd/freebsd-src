# $FreeBSD$

.PATH:	${.CURDIR}/../../net
KMOD=	if_tun
SRCS=	if_tun.c opt_inet.h vnode_if.h
NOMAN=

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.include <bsd.kmod.mk>
