# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/tbl/Makefile,v 1.1.2.3 2003/06/26 13:13:56 ru Exp $

PROG_CXX=	tbl
SRCS=		main.cpp table.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
