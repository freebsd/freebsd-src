# $FreeBSD$
# $Whistle: Makefile,v 1.1 1999/12/03 01:44:28 archie Exp $

KMOD=		ng_bpf
SRCS= 		ng_bpf.c bpf_filter.c
KMODDEPS=	netgraph

.PATH:		${.CURDIR}/../../../net

.include <bsd.kmod.mk>
