# $FreeBSD$

PROG_CXX=	grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
