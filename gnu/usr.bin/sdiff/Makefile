# $Id: Makefile,v 1.7 1997/02/22 15:45:24 peter Exp $

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
