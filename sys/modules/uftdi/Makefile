# $FreeBSD: src/sys/modules/uftdi/Makefile,v 1.4 2004/12/29 08:49:49 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uftdi
SRCS=	uftdi.c uftdireg.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
