# Makefile for grops

PROG=		grops
SRCS=		ps.cc psrm.cc
#CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

MANDEPEND=	grops.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
