# $FreeBSD: src/sys/modules/cdce/Makefile,v 1.1.14.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
