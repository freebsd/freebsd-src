#       $Id: Makefile,v 1.16 1994/11/01 17:32:46 ache Exp $


CFLAGS+= -I${.CURDIR} -DVOID_SIGHANDLER -DHAVE_CONFIG_H

LIB=    readline
SRCS+=  readline.c funmap.c keymaps.c vi_mode.c parens.c \
	   rltty.c complete.c bind.c isearch.c display.c signals.c \
	   history.c search.c tilde.c xmalloc.c
HEADERS= history.h readline.h keymaps.h chardefs.h tilde.h
MAN3=   readline.3
DPADD+= $(LIBTERMCAP)
LDADD+= -ltermcap

beforeinstall:
	@-if [ ! -d ${DESTDIR}/usr/include/readline ]; then \
	  mkdir ${DESTDIR}/usr/include/readline; \
	  chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/readline; \
	  chmod 755 ${DESTDIR}/usr/include/readline; \
	fi
	cd ${.CURDIR}; \
	for m in ${HEADERS} ; do \
	  cmp -s $$m ${DESTDIR}/usr/include/readline || \
	  install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$m ${DESTDIR}/usr/include/readline ; \
	done

.include <bsd.lib.mk>
