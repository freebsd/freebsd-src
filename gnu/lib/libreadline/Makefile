#       $Id: Makefile,v 1.1.1.1 1994/05/09 15:59:08 ache Exp $

SHLIB_MAJOR=1
SHLIB_MINOR=0

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/readline -DVOID_SIGHANDLER -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp

LIB=    readline
SRCS+=  readline.c history.c funmap.c keymaps.c
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
