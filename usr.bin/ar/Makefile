#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/ar/Makefile,v 1.13 2002/07/14 18:23:22 alfred Exp $

PROG=	ar
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
CFLAGS+=-I${.CURDIR}
MAN=	ar.1aout ar.5
BINDIR=	/usr/libexec/aout
WARNS?=	4

.include <bsd.prog.mk>
