#	$NetBSD: Makefile,v 1.2 1995/07/03 21:24:39 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/lint2/Makefile,v 1.4 2002/03/03 15:12:22 markm Exp $

.PATH:	${.CURDIR}/../lint1

PROG=	lint2
SRCS=	main2.c hash.c read.c mem.c mem2.c chk.c msg.c emit.c emit2.c inittyp.c
NOMAN=
CFLAGS+=-I${.CURDIR}/../lint1
LINTFLAGS=-abehrz

BINDIR=	/usr/libexec

.include <bsd.prog.mk>
