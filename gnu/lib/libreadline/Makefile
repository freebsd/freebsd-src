#       $Id: Makefile,v 1.20 1995/05/08 08:43:06 ache Exp $


CFLAGS+= -I${.CURDIR} -DVOID_SIGHANDLER -DHAVE_CONFIG_H

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
	cd ${.CURDIR}; \
	for m in ${HEADERS} ; do \
	  cmp -s $$m ${DESTDIR}/usr/include/readline/$$m || \
	  ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$m ${DESTDIR}/usr/include/readline ; \
	done

.include <bsd.lib.mk>
