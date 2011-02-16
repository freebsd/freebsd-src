# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.5.24.1 2010/12/21 17:10:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
