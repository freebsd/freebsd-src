# $FreeBSD$

.include "../Makefile.inc0"

# Not elf specific so don't install in /usr/libexec/elf
BINDIR=/usr/bin

GDBDIR=	${.CURDIR}/../../../../contrib/gdb.291
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb

PROG=	gdbreplay
NOMAN=	yes

SRCS=	gdbreplay.c

CFLAGS+=	-I${.CURDIR}/../gdb/${MACHINE_ARCH}
CFLAGS+=	-I${GDBDIR}/gdb
CFLAGS+=	-I${GDBDIR}/gdb/config
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-DNO_MMALLOC

.include <bsd.prog.mk>
