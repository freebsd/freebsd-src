# $FreeBSD: src/gnu/usr.bin/groff/tbl/Makefile,v 1.5.8.1 2000/12/07 09:57:19 ru Exp $

PROG=		tbl
SRCS=		main.cc table.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
