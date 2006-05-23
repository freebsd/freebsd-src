# $FreeBSD: src/sys/modules/uhid/Makefile,v 1.12 2004/06/27 12:33:18 imp Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	uhid
SRCS=	bus_if.h device_if.h vnode_if.h opt_usb.h uhid.c usbdevs.h

.include <bsd.kmod.mk>
