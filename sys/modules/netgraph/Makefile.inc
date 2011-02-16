# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
