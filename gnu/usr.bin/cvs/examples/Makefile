# $Id: Makefile,v 1.5 1995/02/22 21:49:46 roberto Exp $

CVSFILES= commitinfo loginfo rcsinfo editinfo modules

EXAMPDIR = /usr/share/examples/cvs

NOMAN = noman

all clean:
	@echo -n

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${CVSFILES} \
		${DESTDIR}${EXAMPDIR}

.include <bsd.prog.mk>
