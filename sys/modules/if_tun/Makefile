# $FreeBSD: src/sys/modules/if_tun/Makefile,v 1.19.2.1 2001/07/24 09:49:42 dd Exp $

.PATH:  ${.CURDIR}/../../net
KMOD=   if_tun
SRCS=   if_tun.c opt_inet.h vnode_if.h
NOMAN=

NBPF?=	1
NTUN?=		2

CFLAGS+= ${PROTOS}

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
