# $FreeBSD: src/sys/modules/umct/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	umct
SRCS=	umct.c ucomvar.h opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
