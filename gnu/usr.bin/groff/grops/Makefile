# Makefile for grops

PROG=		grops
SRCS=		ps.cc psrm.cc
CFLAGS+=	-I$(.CURDIR)/../include
CXXFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBDRIVER) $(LIBGROFF) -lm
DPADD+=		$(LIBDRIVER) $(LIBGROFF) $(LIBMATH)

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
