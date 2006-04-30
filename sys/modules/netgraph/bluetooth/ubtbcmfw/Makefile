# $Id: Makefile,v 1.1 2003/04/27 00:22:12 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

KMOD=		ubtbcmfw
SRCS=		ubtbcmfw.c bus_if.h device_if.h opt_usb.h usbdevs.h

.include <bsd.kmod.mk>

