# $Id: Makefile,v 1.1 2003/04/27 00:22:12 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/ubtbcmfw

#CFLAGS+=	-DINVARIANTS=1 -DINVARIANT_SUPPORT=1
#CFLAGS+=	-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ubtbcmfw
SRCS=		ubtbcmfw.c bus_if.h device_if.h vnode_if.h opt_usb.h
MAN4=		ubtbcmfw.4

.include <bsd.kmod.mk>

