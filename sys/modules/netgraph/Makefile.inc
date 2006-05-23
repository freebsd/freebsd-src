# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.2.8.1 2005/05/27 14:22:30 glebius Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph
CFLAGS+=	-Wall

EXPORT_SYMS=	YES	# XXX evaluate

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
