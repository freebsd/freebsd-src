# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.4.38.1 2010/12/21 17:10:29 kensmith Exp $

PROG_CXX=	grn
SRCS=		hdb.cpp hpoint.cpp hgraph.cpp main.cpp
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
