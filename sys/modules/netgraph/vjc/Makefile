# $FreeBSD: src/sys/modules/netgraph/vjc/Makefile,v 1.1.2.3 1999/12/08 19:44:13 julian Exp $
# $Whistle: Makefile,v 1.1 1999/01/24 06:48:07 archie Exp $

KMOD=		ng_vjc
SRCS= 		ng_vjc.c slcompress.c
MAN8=		ng_vjc.8
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
