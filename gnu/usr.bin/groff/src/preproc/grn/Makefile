# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.3 2002/04/11 11:06:04 ru Exp $

PROG_CXX=	grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
