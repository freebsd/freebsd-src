# $FreeBSD$

PROG=		lookbib
SRCS=		lookbib.cc
LDADD+=		$(LIBBIB) $(LIBGROFF) -lm
DPADD+=		$(LIBBIB) $(LIBGROFF) $(LIBM)

CLEANFILES+=	lookbib.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
