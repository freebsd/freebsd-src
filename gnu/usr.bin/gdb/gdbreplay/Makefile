#	$Id: Makefile,v 1.13.2.1 1997/02/02 20:46:32 joerg Exp $

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

PROG=	gdbreplay
BINDIR=	/usr/bin
NOMAN=	yes

SRCS=	gdbreplay.c

CFLAGS+=-I${.CURDIR}/../gdb -I${.CURDIR}/../bfd -I${GDBDIR}/gdb -I${GDBDIR}/bfd
CFLAGS+=-DNO_MMALLOC

.include <bsd.prog.mk>
