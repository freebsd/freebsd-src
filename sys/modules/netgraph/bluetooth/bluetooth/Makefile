# $Id: Makefile,v 1.1 2002/11/24 20:39:57 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c
MAN4=		ng_bluetooth.4

.include <bsd.kmod.mk>

