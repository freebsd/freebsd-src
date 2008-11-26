# $Id: Makefile,v 1.1 2002/11/24 20:40:04 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/h4/Makefile,v 1.5.20.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

KMOD=		ng_h4
SRCS=		ng_h4.c

.include <bsd.kmod.mk>

