# $Id: Makefile,v 1.6 2002/09/04 21:42:00 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c \
		-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 \
		-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_bt3c
SRCS=		ng_bt3c_pccard.c bus_if.h card_if.h device_if.h
MAN4=		ng_bt3c.4

.include <bsd.kmod.mk>

