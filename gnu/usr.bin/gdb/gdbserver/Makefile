#	$Id: Makefile,v 1.3 1998/05/01 14:48:06 bde Exp $

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

PROG=	gdbserver

SRCS=	remote-utils.c utils.c server.c
SRCS+=	low-fbsd.c

CFLAGS+=-I${.CURDIR}/../gdb -I${.CURDIR}/../bfd
CFLAGS+=-DNO_MMALLOC

.include <bsd.prog.mk>
