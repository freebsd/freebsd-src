# $FreeBSD: src/sys/modules/axe/Makefile,v 1.2.22.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_axe
SRCS=	if_axe.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
