# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.1.2.1 2001/04/26 17:08:30 ru Exp $

PROG=		grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
