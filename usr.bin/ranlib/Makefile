#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1998/10/10 12:48:48 bde Exp $

PROG=	ranlib
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar
MAN1aout=ranlib.1aout
MAN5=	ranlib.5
BINDIR= /usr/libexec/aout
VPATH=	${.CURDIR}/../ar

.include <bsd.prog.mk>
