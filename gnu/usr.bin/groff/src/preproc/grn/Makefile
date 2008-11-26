# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.4.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	grn
SRCS=		hdb.cpp hpoint.cpp hgraph.cpp main.cpp
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
