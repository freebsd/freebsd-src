#	$Id: Makefile,v 1.2 1997/04/26 17:34:04 pst Exp $

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

PROG=	gdbreplay
NOMAN=	yes

SRCS=	gdbreplay.c

CFLAGS+=-I${.CURDIR}/../gdb -I${.CURDIR}/../bfd
CFLAGS+=-DNO_MMALLOC

.include <bsd.prog.mk>
