# $Id: Makefile,v 1.2 2003/01/12 23:37:31 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/socket/Makefile,v 1.3 2003/05/10 21:44:38 julian Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/socket

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include

#CFLAGS+=	-DINVARIANTS=1 -DINVARIANT_SUPPORT=1
#CFLAGS+=	-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c \
		ng_btsocket_rfcomm.c
MAN4=		ng_btsocket.4

.include <bsd.kmod.mk>
