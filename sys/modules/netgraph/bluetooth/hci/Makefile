# $Id: Makefile,v 1.1 2002/11/24 20:40:05 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/hci

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/hci

#CFLAGS+=	-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 

KMOD=		ng_hci
SRCS=		ng_hci_main.c ng_hci_cmds.c ng_hci_evnt.c  \
		ng_hci_ulpi.c ng_hci_misc.c
MAN4=		ng_hci.4

.include <bsd.kmod.mk>
