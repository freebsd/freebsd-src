# $FreeBSD: src/sys/modules/netgraph/bpf/Makefile,v 1.2.2.1 1999/12/06 22:06:55 archie Exp $
# $Whistle: Makefile,v 1.1 1999/12/03 01:44:28 archie Exp $

KMOD=		ng_bpf
SRCS= 		ng_bpf.c bpf_filter.c
MAN8=		ng_bpf.8
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
