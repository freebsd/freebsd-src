# $FreeBSD: src/sys/modules/ubser/Makefile,v 1.2 2004/06/27 12:33:17 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ubser
SRCS=	ubser.c ubser.h opt_usb.h device_if.h bus_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
