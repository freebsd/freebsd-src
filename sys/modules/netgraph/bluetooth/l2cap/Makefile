# $Id: Makefile,v 1.4 2002/09/04 21:38:38 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/l2cap/Makefile,v 1.2 2002/11/21 07:21:44 imp Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/l2cap

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/l2cap \
		-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 \
		-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_l2cap
SRCS=		ng_l2cap_main.c ng_l2cap_cmds.c ng_l2cap_evnt.c \
		ng_l2cap_ulpi.c ng_l2cap_llpi.c ng_l2cap_misc.c
MAN4=		ng_l2cap.4

.include <bsd.kmod.mk>
