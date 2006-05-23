# $FreeBSD: src/sys/modules/cdce/Makefile,v 1.1.2.1 2005/04/01 12:46:27 sobomax Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cdce
SRCS=	if_cdce.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	usbdevs.h

.include <bsd.kmod.mk>
