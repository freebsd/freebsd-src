# $FreeBSD: src/sys/modules/axe/Makefile,v 1.2 2004/06/27 12:33:16 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_axe
SRCS=	if_axe.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
