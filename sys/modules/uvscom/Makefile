# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
