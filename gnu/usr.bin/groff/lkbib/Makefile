# $FreeBSD$

PROG=		lkbib
SRCS=		lkbib.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBBIB} ${LIBGROFF} -lm
DPADD+=		${LIBBIB} ${LIBGROFF} ${LIBM}

CLEANFILES+=	lkbib.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
