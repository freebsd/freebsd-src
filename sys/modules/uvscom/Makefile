# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
