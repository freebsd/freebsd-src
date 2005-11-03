# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.4 2005/05/16 08:25:55 glebius Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
