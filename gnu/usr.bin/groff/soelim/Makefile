# Makefile for soelim

PROG=		soelim
SRCS=		soelim.cc
CFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBGROFF)
DPADD+=		$(LIBGROFF)

.include <bsd.prog.mk>
.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
