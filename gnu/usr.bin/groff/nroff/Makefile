# $FreeBSD$

MAN1=		nroff.1 psroff.1

CLEANFILES+=	${MAN1}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(DIST_DIR)/nroff.sh $(DESTDIR)$(BINDIR)/nroff
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(DIST_DIR)/psroff.sh $(DESTDIR)$(BINDIR)/psroff

.include <../Makefile.cfg>
.include <bsd.prog.mk>
