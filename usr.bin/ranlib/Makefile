#	@(#)Makefile	5.4 (Berkeley) 3/12/91

PROG=	ranlib
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar
MAN1=	ranlib.1
MAN5=	ranlib.5
VPATH=	${.CURDIR}/../ar

.include <bsd.prog.mk>
