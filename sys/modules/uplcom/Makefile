# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.1 2002/03/18 18:23:41 joe Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
