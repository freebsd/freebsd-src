#       @(#)Makefile    5.6 (Berkeley) 6/23/90

FILES=	makewhatis
NOMAN=	noman

CLEANFILES+=	${FILES}

all: ${FILES}

makewhatis: makewhatis.sh
	sed -e 's/%sections%/ "1", "n", "l", "6", "8", "2", "3", "4", "5", "7", "p", "o", NULL/' \
	    -e 's,%compext%,${compext},' \
	    -e 's,%zcat%,${zcat},' \
	    ${.CURDIR}/${.TARGET}.sh > ${.TARGET}

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${FILES} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
