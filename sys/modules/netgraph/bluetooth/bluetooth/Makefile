# $Id: Makefile,v 1.1 2002/11/24 20:39:57 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/bluetooth/Makefile,v 1.5.20.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

.include <bsd.kmod.mk>

