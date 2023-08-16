# $Id: Makefile,v 1.1 2002/11/24 20:39:57 max Exp $

.PATH: ${SRCTOP}/sys/netgraph/bluetooth/common

CFLAGS+=	-I${SRCTOP}/sys/netgraph/bluetooth/include

KMOD=		ng_bluetooth
SRCS=		ng_bluetooth.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
