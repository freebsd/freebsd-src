# $FreeBSD$

PROG=		grops
SRCS=		ps.cc psrm.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grops.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
