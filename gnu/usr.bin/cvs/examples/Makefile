# $Id: Makefile,v 1.1 1995/04/08 17:50:48 nate Exp $

CVSFILES= commitinfo loginfo rcsinfo editinfo modules

EXAMPDIR = /usr/share/examples/cvs

NOMAN = noman

all clean:
	@echo -n

install:
	cd ${.CURDIR}; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${CVSFILES} \
		${DESTDIR}${EXAMPDIR}

.include <bsd.prog.mk>
