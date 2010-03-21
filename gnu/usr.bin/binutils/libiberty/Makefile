# $FreeBSD: src/gnu/usr.bin/binutils/libiberty/Makefile,v 1.24.24.1 2010/02/10 00:26:20 kensmith Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c concat.c choose-temp.c cp-demangle.c cp-demint.c		\
	cplus-dem.c dyn-string.c getpwd.c getruntime.c floatformat.c	\
	hashtab.c hex.c lbasename.c lrealpath.c make-relative-prefix.c	\
	make-temp-file.c objalloc.c obstack.c safe-ctype.c xatexit.c	\
	xexit.c xmalloc.c xstrdup.c xstrerror.c

# The following files are needed by gdb(1)
SRCS+=	splay-tree.c

WARNS?=	1
CFLAGS+=	-DHAVE_CONFIG_H
INTERNALLIB=

.include <bsd.lib.mk>
