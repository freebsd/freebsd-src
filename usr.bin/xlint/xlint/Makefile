#	$NetBSD: Makefile,v 1.11 2001/10/25 02:22:56 thorpej Exp $

.PATH:		${.CURDIR}/../lint1

PROG=		xlint
SRCS=		xlint.c mem.c
PROGNAME= 	lint
MAN=		lint.1

CPPFLAGS+=	-I${.CURDIR}/../lint1

.ifndef HOSTPROG
.include <bsd.prog.mk>
.endif
