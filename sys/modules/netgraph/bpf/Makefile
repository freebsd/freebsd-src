# $FreeBSD: src/sys/modules/netgraph/bpf/Makefile,v 1.2.4.1 2000/05/04 18:42:52 archie Exp $
# $Whistle: Makefile,v 1.1 1999/12/03 01:44:28 archie Exp $

KMOD=		ng_bpf
SRCS= 		ng_bpf.c bpf_filter.c
MAN4=		ng_bpf.4
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
