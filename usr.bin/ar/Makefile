#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/ar/Makefile,v 1.9 1999/08/28 00:59:05 peter Exp $

PROG=	ar
CFLAGS+=-I${.CURDIR} -Wall
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
MAN1aout=ar.1aout
MAN5=	ar.5
BINDIR=	/usr/libexec/aout

.include <bsd.prog.mk>
