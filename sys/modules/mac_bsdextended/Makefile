# $FreeBSD: src/sys/modules/mac_bsdextended/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../security/mac_bsdextended

KMOD=	mac_bsdextended
SRCS=	vnode_if.h \
	mac_bsdextended.h \
	mac_bsdextended.c \
	ugidfw_system.c \
	ugidfw_vnode.c

.include <bsd.kmod.mk>
