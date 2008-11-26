# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
