# $FreeBSD: src/sys/modules/cdce/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
