# $FreeBSD: src/gnu/usr.bin/groff/grn/Makefile,v 1.1.2.1 2000/12/07 09:57:02 ru Exp $

PROG=		grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBGROFF} -lm
DPADD+= 	${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
