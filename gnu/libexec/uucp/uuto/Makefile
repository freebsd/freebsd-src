# Makefile for uuto
# $Id: Makefile,v 1.9 1997/09/13 21:44:48 wosch Exp $

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
