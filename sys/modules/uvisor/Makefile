# $FreeBSD: src/sys/modules/uvisor/Makefile,v 1.3 2004/06/27 12:33:20 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvisor
SRCS=	uvisor.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
