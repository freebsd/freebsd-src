#       $Id: Makefile,v 1.21 1995/08/06 12:22:57 bde Exp $


CFLAGS+= -I${.CURDIR} -DVOID_SIGHANDLER -DHAVE_CONFIG_H \
	-DHAVE_STDLIB_H -DHAVE_UNISTD_H

LIB=    readline
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
SRCS+=  readline.c funmap.c keymaps.c vi_mode.c parens.c \
	   rltty.c complete.c bind.c isearch.c display.c signals.c \
	   history.c search.c tilde.c xmalloc.c
HEADERS= history.h readline.h keymaps.h chardefs.h tilde.h
MAN3=   readline.3
DPADD+= $(LIBTERMCAP)
LDADD+= -ltermcap

SUBDIR+= doc

beforeinstall:
.for i in ${HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$i \
		${DESTDIR}/usr/include/readline
.endfor

.include <bsd.lib.mk>
