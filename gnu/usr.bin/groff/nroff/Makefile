MAN1=		nroff.1
MANDEPEND=	nroff.1

CLEANFILES+=	${MANDEPEND}

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(.CURDIR)/nroff.sh $(DESTDIR)$(BINDIR)/nroff
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(.CURDIR)/psroff.sh $(DESTDIR)$(BINDIR)/psroff

.include <../Makefile.cfg>
.include <bsd.prog.mk>
