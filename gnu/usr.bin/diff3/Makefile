# $FreeBSD: src/gnu/usr.bin/diff3/Makefile,v 1.7 2004/02/17 01:28:48 ache Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=		diff3
SRCS=           diff3.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDIFF_PROGRAM=\"/usr/bin/diff\"

.include <bsd.prog.mk>
