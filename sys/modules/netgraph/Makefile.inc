# $FreeBSD: src/sys/modules/netgraph/Makefile.inc,v 1.2 2002/01/11 01:19:25 msmith Exp $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph
CFLAGS+=	-Wall

EXPORT_SYMS=	YES	# XXX evaluate

.include "../Makefile.inc"
