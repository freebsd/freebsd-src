#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/restore/Makefile,v 1.24.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../dump

PROG=	restore
LINKS=	${BINDIR}/restore ${BINDIR}/rrestore
CFLAGS+=-DRRESTORE
WARNS?=	0
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c \
	dumprmt.c
MAN=	restore.8
MLINKS=	restore.8 rrestore.8

.include <bsd.prog.mk>
