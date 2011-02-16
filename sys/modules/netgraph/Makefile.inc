# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
