# $FreeBSD$

PROG=		tfmtodit
SRCS=		tfmtodit.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	tfmtodit.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
