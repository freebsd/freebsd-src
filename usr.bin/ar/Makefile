#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	ar
CFLAGS+=-I${.CURDIR} -Wall
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
MAN=	ar.1aout ar.5
BINDIR=	/usr/libexec/aout

.include <bsd.prog.mk>
