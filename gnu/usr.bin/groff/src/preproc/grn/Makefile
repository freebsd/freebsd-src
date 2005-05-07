# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.4 2003/05/01 13:22:19 ru Exp $

PROG_CXX=	grn
SRCS=		hdb.cpp hpoint.cpp hgraph.cpp main.cpp
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
