# $Id: Makefile,v 1.9 1997/10/29 17:35:54 ache Exp $

.PATH:  ${.CURDIR}/../../../contrib/patch

PROG= 	patch

LIBSRCS = getopt.c getopt1.c
SRCS = addext.c argmatch.c backupfile.c basename.c inp.c maketime.c \
	partime.c patch.c pch.c quotearg.c util.c version.c $(LIBSRCS)

CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${.CURDIR}/../../../contrib/patch
CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -Ded_PROGRAM=\"/bin/ed\"

.include <bsd.prog.mk>
