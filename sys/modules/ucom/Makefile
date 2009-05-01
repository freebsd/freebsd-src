# $FreeBSD: src/sys/modules/ucom/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucom
SRCS=	ucom.c ucomvar.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
