# Makefile for refer

PROG=		refer
SRCS=		command.cc ref.cc refer.cc token.cc
OBJS=		label.o
CFLAGS+=	-I. -I$(.CURDIR)/../include
LDADD+=		$(LIBBIB) $(LIBGROFF) -lm
DPADD+=		$(LIBBIB) $(LIBGROFF) $(LIBMATH)

CLEANFILES+=	label.cc label.tab.h

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
