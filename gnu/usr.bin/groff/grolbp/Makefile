# $FreeBSD$

PROG=		grolbp
SRCS=		lbp.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grolbp.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
