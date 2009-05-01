# $Id: Makefile,v 1.1 2002/11/24 20:39:57 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bluetooth/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

.include <bsd.kmod.mk>
