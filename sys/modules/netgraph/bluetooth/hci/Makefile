# $Id: Makefile,v 1.5 2002/09/04 21:36:51 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/hci/Makefile,v 1.2 2002/11/21 07:21:44 imp Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/hci

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/hci \
		-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 \
		-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c
MAN4=		ng_hci.4

.include <bsd.kmod.mk>
