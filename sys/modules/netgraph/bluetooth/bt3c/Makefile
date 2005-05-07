# $Id: Makefile,v 1.2 2002/11/24 20:50:56 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bt3c/Makefile,v 1.6 2004/05/26 00:53:05 imp Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

KMOD=		ng_bt3c
SRCS=		ng_bt3c_pccard.c bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>

