# $FreeBSD$

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

PROG=	gdbserver
BINDIR=	/usr/bin

SRCS=	remote-utils.c utils.c server.c
SRCS+=	low-fbsd.c

CFLAGS+=-I${.CURDIR}/../gdb -I${.CURDIR}/../bfd
CFLAGS+=-DNO_MMALLOC

.include <bsd.prog.mk>
