# Makefile for uuto
# $Id: Makefile,v 1.6 1995/07/25 00:31:45 bde Exp $

BINDIR=		$(bindir)
CLEANFILES+=	$(ONESHPROG)
ONESHPROG=	uuto
MAN1=		uuto.1

all: $(ONESHPROG)

$(ONESHPROG): $(ONESHPROG).in Makefile
	sed -e "s|@BINDIR@|$(bindir)|g" \
		$(.ALLSRC:N*Makefile) > $(.TARGET)

beforeinstall:
	${INSTALL} -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(ONESHPROG) $(DESTDIR)$(BINDIR)/$(ONESHPROG)

.include <bsd.prog.mk>
