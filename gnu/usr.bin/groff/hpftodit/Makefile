# $FreeBSD$

PROG=		hpftodit
SRCS=		hpftodit.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	hpftodit.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
