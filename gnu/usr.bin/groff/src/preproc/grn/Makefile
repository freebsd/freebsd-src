# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.1.2.3 2003/06/26 13:13:55 ru Exp $

PROG_CXX=	grn
SRCS=		hdb.cpp hpoint.cpp hgraph.cpp main.cpp
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
