#	Makefile,v 1.1 1994/09/13 23:01:57 wollman Exp

NOPROG=	tzsetup
NOMAN=

all depend:

beforeinstall:
	$(INSTALL) -c -m $(BINMODE) -o $(BINOWN) -g $(BINGRP) \
		${.CURDIR}/tzsetup.sh ${DESTDIR}${BINDIR}/tzsetup

.include <bsd.prog.mk>
