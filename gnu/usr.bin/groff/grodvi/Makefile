# $FreeBSD$

PROG=		grodvi
SRCS=		dvi.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grodvi.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
