#       $Id: Makefile,v 1.4 1994/05/09 23:23:40 ache Exp $

SHLIB_MAJOR=1
SHLIB_MINOR=0

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/readline -DVOID_SIGHANDLER \
	 -DHAVE_UNISTD_H -DHAVE_STRING_H -DHAVE_STDLIB_H \
	 -DVI_MODE -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp

LIB=    readline
SRCS+=  readline.c funmap.c keymaps.c vi_mode.c parens.c \
	   rltty.c complete.c bind.c isearch.c display.c signals.c \
	   history.c search.c tilde.c xmalloc.c
NOMAN=	noman

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/readline ]; then \
	  mkdir ${DESTDIR}/usr/include/readline; \
	  chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/readline; \
	  chmod 755 ${DESTDIR}/usr/include/readline; \
	fi
	cd ${.CURDIR}/readline; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	history.h readline.h keymaps.h chardefs.h ${DESTDIR}/usr/include/readline

.include <bsd.lib.mk>
