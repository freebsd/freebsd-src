# $FreeBSD$
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph
CFLAGS+=	-Wall

EXPORT_SYMS=	YES	# XXX evaluate

.include "../Makefile.inc"
