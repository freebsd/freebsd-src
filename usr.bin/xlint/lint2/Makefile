#	$NetBSD: Makefile,v 1.2 1995/07/03 21:24:39 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/lint2/Makefile,v 1.5.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../lint1

PROG=	lint2
SRCS=	main2.c hash.c read.c mem.c mem2.c chk.c msg.c emit.c emit2.c inittyp.c
NO_MAN=
CFLAGS+=-I${.CURDIR}/../lint1
LINTFLAGS=-abehrz

BINDIR=	/usr/libexec

.include <bsd.prog.mk>
