# Makefile for which
# $Id: Makefile,v 1.6 1995/01/24 17:58:35 bde Exp $

NOOBJ=	yes
SRCS=

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		which.pl $(DESTDIR)$(BINDIR)/which

.include <bsd.prog.mk>
