# $FreeBSD: src/sys/modules/netgraph/vjc/Makefile,v 1.2.2.1 2000/05/04 18:44:03 archie Exp $
# $Whistle: Makefile,v 1.1 1999/01/24 06:48:07 archie Exp $

KMOD=		ng_vjc
SRCS= 		ng_vjc.c slcompress.c
MAN4=		ng_vjc.4
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
