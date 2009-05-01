# $FreeBSD: src/sys/modules/rue/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_rue
SRCS=	if_rue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
