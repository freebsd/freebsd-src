# $FreeBSD$

PROG=		tbl
SRCS=		main.cc table.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	tbl.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
