#       $Id: Makefile,v 1.24 1997/01/05 16:16:32 ache Exp $

SRCDIR= ${.CURDIR}/../../../contrib/libreadline
.PATH: ${SRCDIR}

#SUBDIR= doc

LIB=    readline
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
MAN3=   readline.3

CFLAGS+= -I${.CURDIR} -I${SRCDIR} -DHAVE_CONFIG_H

HISTSRC=  history.c histexpand.c histfile.c histsearch.c
TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  nls.c xmalloc.c \
	  $(HISTSRC) $(TILDESRC)

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h

DPADD+= $(LIBTERMCAP)
LDADD+= -ltermcap

beforeinstall:
.for i in ${INSTALLED_HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${SRCDIR}/$i \
		${DESTDIR}/usr/include/readline
.endfor

.include <bsd.lib.mk>
