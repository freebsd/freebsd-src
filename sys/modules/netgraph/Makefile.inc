# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
