LIB = forms

SRCS = forms.c yacc.c lex.c
CLEANFILES += y.tab.h lex.c yacc.c

CFLAGS = -I. -I${.CURDIR}

beforeinstall:
	@(cd ${.CURDIR}; cmp -s forms.h ${DESTDIR}/usr/include/forms.h || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 forms.h \
		${DESTDIR}/usr/include/forms.h;)

.include <bsd.lib.mk>
