# $Id: Makefile,v 1.1.1.1 2002/09/04 21:47:41 max Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/common

CFLAGS+=	-I../../../../netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c
MAN4=		ng_bluetooth.4

.include <bsd.kmod.mk>

