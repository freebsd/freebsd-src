# $FreeBSD: src/sys/modules/umct/Makefile,v 1.1 2003/06/28 05:45:55 scottl Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	umct
SRCS=	umct.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
