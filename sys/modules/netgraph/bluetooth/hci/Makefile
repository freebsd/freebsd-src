# $Id: Makefile,v 1.1 2002/11/24 20:40:05 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/hci/Makefile,v 1.4 2003/10/12 22:04:19 emax Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/hci

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/hci

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c
MAN4=		ng_hci.4

.include <bsd.kmod.mk>
