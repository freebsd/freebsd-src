# Makefile for lookbib

PROG=		lookbib
SRCS=		lookbib.cc
CFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBBIB) $(LIBGROFF) -lm
DPADD+=		$(LIBBIB) $(LIBGROFF) $(LIBMATH)

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
