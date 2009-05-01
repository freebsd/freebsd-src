# $FreeBSD: src/sys/modules/cdce/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
