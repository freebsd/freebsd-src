# $FreeBSD: src/sys/modules/netgraph/bpf/Makefile,v 1.2.4.3 2001/12/21 09:00:47 ru Exp $
# $Whistle: Makefile,v 1.1 1999/12/03 01:44:28 archie Exp $

KMOD=		ng_bpf
SRCS= 		ng_bpf.c bpf_filter.c
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
