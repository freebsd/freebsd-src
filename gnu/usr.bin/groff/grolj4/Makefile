# $FreeBSD$

PROG=		grolj4
SRCS=		lj4.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grolj4.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
