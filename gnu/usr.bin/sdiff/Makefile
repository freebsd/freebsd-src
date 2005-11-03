# $FreeBSD: src/gnu/usr.bin/sdiff/Makefile,v 1.6 2004/02/17 01:30:56 ache Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=		sdiff
SRCS=           sdiff.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDIFF_PROGRAM=\"/usr/bin/diff\"
CFLAGS+=-DDEFAULT_EDITOR_PROGRAM=\"/usr/bin/vi\"

.include <bsd.prog.mk>
