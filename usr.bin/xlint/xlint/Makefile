#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:14 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/xlint/Makefile,v 1.8.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../lint1

PROG=	xlint
PROGNAME=	lint
SRCS=	xlint.c mem.c
MAN=	lint.1

CFLAGS+=-I${.CURDIR}/../lint1
CFLAGS+= -DPREFIX=\"${TOOLS_PREFIX}\"

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
