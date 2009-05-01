# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.4.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	grn
SRCS=		hdb.cpp hpoint.cpp hgraph.cpp main.cpp
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
