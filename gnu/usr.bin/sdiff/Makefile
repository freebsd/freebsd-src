# $FreeBSD: src/gnu/usr.bin/sdiff/Makefile,v 1.4.2.1 1999/08/29 14:35:17 peter Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=		sdiff
SRCS=		sdiff.c getopt.c getopt1.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDIFF_PROGRAM=\"/usr/bin/diff\"
CFLAGS+=-DDEFAULT_EDITOR_PROGRAM=\"/usr/bin/vi\"

.include <bsd.prog.mk>
