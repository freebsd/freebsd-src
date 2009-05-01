# $FreeBSD: src/gnu/usr.bin/binutils/gdbreplay/Makefile,v 1.15.20.1 2009/04/15 03:14:26 kensmith Exp $

.include "../Makefile.inc0"

# Not elf specific so don't install in /usr/libexec/elf
BINDIR=/usr/bin

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb

PROG=	gdbreplay
NO_MAN=

SRCS=	gdbreplay.c

CFLAGS+=	-I${.CURDIR}/../gdb
CFLAGS+=	-I${GDBDIR}/gdb
CFLAGS+=	-I${GDBDIR}/gdb/config
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-DNO_MMALLOC

.include <bsd.prog.mk>
