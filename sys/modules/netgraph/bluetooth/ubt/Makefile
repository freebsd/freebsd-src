# $Id: Makefile,v 1.5 2002/09/04 21:41:06 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubt \
		-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 \
		-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_ubt
SRCS=		ng_ubt.c bus_if.h device_if.h opt_usb.h
MAN4=		ng_ubt.4

.include <bsd.kmod.mk>

