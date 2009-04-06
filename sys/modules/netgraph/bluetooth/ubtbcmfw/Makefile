# $Id: Makefile,v 1.1 2003/04/27 00:22:12 max Exp $
# $FreeBSD$

S=	${.CURDIR}/../../../..

.PATH: $S/dev/usb/bluetooth

KMOD=	ubtbcmfw
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h vnode_if.h usbdevs.h \
	ubtbcmfw.c

.include <bsd.kmod.mk>
