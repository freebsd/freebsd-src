#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.7 1998/10/10 12:48:48 bde Exp $

PROG=	ar
CFLAGS+=-I${.CURDIR} -Wall
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
MAN1aout=ar.1aout
MAN5=	ar.5
BINDIR=	/usr/libexec/aout

.include <bsd.prog.mk>
