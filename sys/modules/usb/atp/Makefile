# $FreeBSD: src/sys/modules/usb/atp/Makefile,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb/input

KMOD=	atp
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h vnode_if.h usbdevs.h \
	atp.c

.include <bsd.kmod.mk>
