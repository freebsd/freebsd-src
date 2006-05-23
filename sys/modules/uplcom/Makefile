# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.4 2004/06/27 12:33:19 imp Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
