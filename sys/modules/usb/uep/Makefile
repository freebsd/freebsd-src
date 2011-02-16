# $FreeBSD: src/sys/modules/usb/uep/Makefile,v 1.1.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb/input

KMOD=	uep
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h vnode_if.h usbdevs.h \
	uep.c

.include <bsd.kmod.mk>
