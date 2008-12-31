# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
