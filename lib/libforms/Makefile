LIB = forms

SRCS = forms.c parser.y lex.l menu.c fields.c

CFLAGS += -I. -I${.CURDIR} -Wall -g -DHASH_STATS
LDFLAGS += -ll

CLEANFILES+= lex.c parser.c y.tab.h

beforeinstall:
	@(cd ${.CURDIR}; cmp -s forms.h ${DESTDIR}/usr/include/forms.h || \
	   install -c -o ${BINOWN} -g ${BINGRP} -m 444 forms.h \
	   ${DESTDIR}/usr/include/forms.h;)

.include <bsd.lib.mk>
