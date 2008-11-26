# $FreeBSD: src/sys/modules/ucom/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucom
SRCS=	ucom.c ucomvar.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
