#	$NetBSD: Makefile,v 1.12 2001/11/12 23:16:37 tv Exp $

.PATH:		${.CURDIR}/../lint1
.PATH:		${.CURDIR}/../../mkdep

PROG=		xlint
SRCS=		xlint.c mem.c findcc.c
PROGNAME= 	lint
MAN=		lint.1

CPPFLAGS+=	-I${.CURDIR}/../lint1
CPPFLAGS+=	-I${.CURDIR}/../../mkdep

.ifndef HOSTPROG
.include <bsd.prog.mk>
.endif
