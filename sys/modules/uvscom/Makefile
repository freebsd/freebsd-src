# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.4 2004/06/27 12:33:20 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
