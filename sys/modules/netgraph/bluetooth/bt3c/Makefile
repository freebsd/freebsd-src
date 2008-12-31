# $Id: Makefile,v 1.2 2002/11/24 20:50:56 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bt3c/Makefile,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/bt3c

KMOD=		ng_bt3c
SRCS=		ng_bt3c_pccard.c bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
