# $Id: Makefile,v 1.5 1997/10/29 16:50:42 ache Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=		diff3
SRCS=		diff3.c getopt.c getopt1.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDIFF_PROGRAM=\"/usr/bin/diff\"

.include <bsd.prog.mk>
