#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	ar
BINDIR=	/usr/libexec/aout
CFLAGS+=-I${.CURDIR}
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
MAN1=	ar.1
MAN5=	ar.5

.include <bsd.prog.mk>
