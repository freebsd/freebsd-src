# $FreeBSD$

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
