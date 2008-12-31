# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/tbl/Makefile,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	tbl
SRCS=		main.cpp table.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
