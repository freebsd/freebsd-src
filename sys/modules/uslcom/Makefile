# $FreeBSD: src/sys/modules/uslcom/Makefile,v 1.2.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uslcom
SRCS=	uslcom.c opt_usb.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
