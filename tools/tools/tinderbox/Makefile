# $FreeBSD$

BINDIR		?= ${HOME}/bin
BINOWN		?= ${USER}
BINGRP		?= ${USER}
SCRIPTS		 = tbmaster.pl tinderbox.pl

MANDIR		?= ${HOME}/man/man
MANOWN		?= ${USER}
MANGRP		?= ${USER}
MAN		 = tbmaster.1 tinderbox.1

SUBDIR		 = www

RCFILES		 = default.rc releng_4.rc current.rc
RCFILES		+= ada.rc dsa.rc dwp.rc stanley.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

beforeinstall:
	(cd ${.CURDIR} &&\
	    ${INSTALL} -m 0644 -o ${ETCOWN} -g ${ETCGRP} ${RCFILES} ${ETCDIR})

.include <bsd.prog.mk>
