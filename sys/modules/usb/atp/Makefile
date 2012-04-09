# $FreeBSD: src/sys/modules/usb/atp/Makefile,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb/input

KMOD=	atp
SRCS=	opt_bus.h opt_usb.h device_if.h bus_if.h usb_if.h vnode_if.h usbdevs.h \
	atp.c

.include <bsd.kmod.mk>
