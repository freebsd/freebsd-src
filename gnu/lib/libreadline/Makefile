#       $Id: Makefile,v 1.13 1994/06/12 08:50:39 ache Exp $


CFLAGS+= -I${.CURDIR} -DVOID_SIGHANDLER \
	 -DHAVE_UNISTD_H -DHAVE_STRING_H -DHAVE_STDLIB_H -DHAVE_VARARGS_H \
	 -DFreeBSD

LIB=    readline
SRCS+=  readline.c funmap.c keymaps.c vi_mode.c parens.c \
	   rltty.c complete.c bind.c isearch.c display.c signals.c \
	   history.c search.c tilde.c xmalloc.c
HEADERS= history.h readline.h keymaps.h chardefs.h tilde.h
NOMAN=	noman

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/readline ]; then \
	  mkdir ${DESTDIR}/usr/include/readline; \
	  chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/readline; \
	  chmod 755 ${DESTDIR}/usr/include/readline; \
	fi
	cd ${.CURDIR}/readline; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HEADERS} ${DESTDIR}/usr/include/readline

.include <bsd.lib.mk>
