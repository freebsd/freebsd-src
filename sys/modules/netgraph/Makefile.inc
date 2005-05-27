# $FreeBSD$
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph
CFLAGS+=	-Wall

EXPORT_SYMS=	YES	# XXX evaluate

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
