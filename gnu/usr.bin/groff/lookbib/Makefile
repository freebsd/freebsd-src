# Makefile for lookbib

PROG=		lookbib
SRCS=		lookbib.cc
CFLAGS+=	-I$(.CURDIR)/../include
LDADD+=		$(LIBBIB) $(LIBGROFF) -lm
DPADD+=		$(LIBBIB) $(LIBGROFF) $(LIBMATH)

MANDEPEND=	lookbib.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
