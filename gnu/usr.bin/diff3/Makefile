# $FreeBSD: src/gnu/usr.bin/diff3/Makefile,v 1.6 1999/08/27 23:35:34 peter Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=		diff3
SRCS=		diff3.c getopt.c getopt1.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDIFF_PROGRAM=\"/usr/bin/diff\"

.include <bsd.prog.mk>
