#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	ranlib
BINDIR= /usr/libexec/aout
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar
MAN1=	ranlib.1
MAN5=	ranlib.5
VPATH=	${.CURDIR}/../ar

.include <bsd.prog.mk>
