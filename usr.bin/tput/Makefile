#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	tput
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
MLINKS=	tput.1 clear.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/clear.sh ${DESTDIR}${BINDIR}/clear

.include <bsd.prog.mk>
