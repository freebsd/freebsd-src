#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:14 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/xlint/Makefile,v 1.3.8.2 2001/04/25 13:07:07 ru Exp $

.PATH:	${.CURDIR}/../lint1

PROG=	xlint
PROGNAME=lint
SRCS=	xlint.c mem.c
MAN=	lint.1

CFLAGS+=-I${.CURDIR}/../lint1

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
