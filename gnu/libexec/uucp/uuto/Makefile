# Makefile for uuto
# $FreeBSD: src/gnu/libexec/uucp/uuto/Makefile,v 1.10 1999/08/27 23:34:04 peter Exp $

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
