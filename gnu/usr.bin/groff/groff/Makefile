# Makefile for groff

PROG=		groff
SRCS=		groff.cc pipeline.c
CFLAGS+=	-I$(.CURDIR)/../include
CXXFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBGROFF) -lm
DPADD+=		$(LIBGROFF) $(LIBMATH)

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
