# Makefile for uusched
# $Id: Makefile,v 1.11 1997/09/14 13:04:25 wosch Exp $

BINDIR=		$(bindir)
CLEANFILES+=	$(ONESHPROG)
ONESHPROG=	uusched
MAN8=		uusched.8

all: $(ONESHPROG)

$(ONESHPROG): $(ONESHPROG).in Makefile
	sed -e "s|@SBINDIR@|$(libxdir)|g" \
		$(.ALLSRC:N*Makefile) > $(.TARGET)

beforeinstall:
	${INSTALL} -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(ONESHPROG) $(DESTDIR)$(BINDIR)/$(ONESHPROG)

.include <bsd.prog.mk>
