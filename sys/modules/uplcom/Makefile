# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.5 2004/12/29 08:49:50 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
