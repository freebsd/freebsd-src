#	$Id$

NOPROG=	tzsetup
NOMAN=

all depend:

beforeinstall:
	$(INSTALL) -c -m $(BINMODE) -o $(BINOWN) -g $(BINGRP) \
		${.CURDIR}/tzsetup.sh ${DESTDIR}${BINDIR}/tzsetup

.include <bsd.prog.mk>
