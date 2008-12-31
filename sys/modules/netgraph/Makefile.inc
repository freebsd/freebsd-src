# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
