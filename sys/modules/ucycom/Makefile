# $FreeBSD: src/sys/modules/ucycom/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucycom
SRCS=	ucycom.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
