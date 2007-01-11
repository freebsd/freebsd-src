# $FreeBSD: src/sys/modules/ucom/Makefile,v 1.4 2004/12/29 08:49:49 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucom
SRCS=	ucom.c ucomvar.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
