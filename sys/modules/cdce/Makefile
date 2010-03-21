# $FreeBSD: src/sys/modules/cdce/Makefile,v 1.2.10.1 2010/02/10 00:26:20 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
