# $FreeBSD$

PROG=		grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBGROFF} -lm
DPADD+= 	${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
