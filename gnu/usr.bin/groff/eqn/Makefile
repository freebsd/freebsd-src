# Makefile for eqn

PROG=		eqn
SRCS=		main.cc lex.cc  box.cc limit.cc list.cc over.cc text.cc\
		script.cc mark.cc other.cc delim.cc sqrt.cc pile.cc special.cc
OBJS=		eqn.o 
CFLAGS+=	-I. -I$(.CURDIR)/../include
CXXFLAGS+=	-I. -I$(.CURDIR)/../include
LDADD+=		$(LIBGROFF)
DPADD+=		$(LIBGROFF)

CLEANFILES+=	eqn.cc eqn.tab.h

afterinstall:
	sed -e 's/@g@/$(g)/g' $(.CURDIR)/neqn.sh > $(DESTDIR)$(BINDIR)/neqn
	chown ${BINOWN}.${BINGRP} $(DESTDIR)$(BINDIR)/neqn
	chmod ${BINMODE} $(DESTDIR)$(BINDIR)/neqn

beforedepend: eqn.cc

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
