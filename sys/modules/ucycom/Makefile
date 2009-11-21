# $FreeBSD: src/sys/modules/ucycom/Makefile,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucycom
SRCS=	ucycom.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
