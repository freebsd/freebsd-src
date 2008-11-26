# $FreeBSD: src/sys/modules/cue/Makefile,v 1.6.20.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cue
SRCS=	if_cue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
