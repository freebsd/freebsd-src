# $Id: Makefile,v 1.2 2003/01/12 23:37:31 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/socket/Makefile,v 1.5.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/socket

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c \
		ng_btsocket_rfcomm.c

.include <bsd.kmod.mk>
