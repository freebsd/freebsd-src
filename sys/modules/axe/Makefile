# $FreeBSD: src/sys/modules/axe/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_axe
SRCS=	if_axe.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
