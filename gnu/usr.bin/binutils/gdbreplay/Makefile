#	$Id: Makefile,v 1.1.1.1 1997/04/25 22:25:22 pst Exp $

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

PROG=	gdbreplay
BINDIR=	/usr/bin
NOMAN=	yes

SRCS=	gdbreplay.c

CFLAGS+=-I${.CURDIR}/../gdb -I${.CURDIR}/../bfd
CFLAGS+=-DNO_MMALLOC

.include <bsd.prog.mk>
