LIB = forms
SHLIB_MAJOR= 3
SHLIB_MINOR= 0

SRCS = debug.c bindings.c parser.y lex.l forms.c objects.c ncurses.c

CFLAGS += -I. -I${.CURDIR} -DHASH_STATS #-g -DDEBUG -Wall -ansi -pedantic -Dlint
LDFLAGS += -ll

CLEANFILES+= lex.c parser.c parser.tab.h

beforeinstall:
	@(cd ${.CURDIR}; cmp -s forms.h ${DESTDIR}/usr/include/forms.h || \
	   ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 forms.h \
	   ${DESTDIR}/usr/include/forms.h;)

.include <bsd.lib.mk>
