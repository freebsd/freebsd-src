#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/bin/rcp/Makefile,v 1.22 2002/10/16 16:06:46 markm Exp $

PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
