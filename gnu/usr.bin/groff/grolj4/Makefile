# Makefile for grolj4

PROG=		grolj4
SRCS=		lj4.cc
#CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

MANDEPEND=	grolj4.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
