# $Id: Makefile,v 1.1 2002/11/24 20:40:04 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/h4/Makefile,v 1.3 2003/05/10 21:44:38 julian Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/drivers/h4

#CFLAGS+=	-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 

KMOD=		ng_h4
SRCS=		ng_h4.c
MAN4=		ng_h4.4

.include <bsd.kmod.mk>

