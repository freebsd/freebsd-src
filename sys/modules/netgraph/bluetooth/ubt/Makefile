# $Id: Makefile,v 1.2 2003/03/22 23:44:34 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/ubt/Makefile,v 1.8.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

KMOD=		ng_ubt
SRCS=		ng_ubt.c bus_if.h device_if.h opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
