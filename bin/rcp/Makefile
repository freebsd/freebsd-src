#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

.include <src.opts.mk>

PACKAGE=rcmds
PROG=	rcp
SRCS=	rcp.c util.c
CFLAGS+=-DBINDIR=${BINDIR}

PACKAGE=rcmds

BINOWN=	root
BINMODE=4555

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
