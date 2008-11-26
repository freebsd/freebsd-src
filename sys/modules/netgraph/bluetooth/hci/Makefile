# $Id: Makefile,v 1.1 2002/11/24 20:40:05 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/hci/Makefile,v 1.5.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/hci

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/hci

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c

.include <bsd.kmod.mk>
