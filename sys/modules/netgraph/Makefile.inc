# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.1 1999/10/21 09:05:28 julian Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph
CFLAGS+=	-Wall

.include "../Makefile.inc"
