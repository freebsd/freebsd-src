# $Id: Makefile,v 1.1.4.1 1996/02/23 12:18:32 peter Exp $

CVSFILES= checkoutlist commitinfo cvswrappers editinfo loginfo modules \
	  rcsinfo rcstemplate taginfo unwrap wrap

EXAMPDIR = /usr/share/examples/cvs

NOMAN = noman
NOOBJ = noobj

all clean:
	@echo -n

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${CVSFILES} \
		${DESTDIR}${EXAMPDIR}/examples

.include <bsd.prog.mk>
