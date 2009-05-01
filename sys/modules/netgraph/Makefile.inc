# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
