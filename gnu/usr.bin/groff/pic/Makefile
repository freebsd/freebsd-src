# Makefile for pic

PROG=		pic
SRCS=		lex.cc main.cc object.cc common.cc troff.cc tex.cc
OBJS=		pic.o
CFLAGS+=	-I. -I$(.CURDIR)/../include
CXXFLAGS+=	-I. -I$(.CURDIR)/../include
LDADD+=		$(LIBGROFF) -lm
DPADD+=		$(LIBGROFF) $(LIBMATH)

CLEANFILES+=	pic.cc pic.tab.h

beforedepend: pic.cc

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
