# Makefile for tfmtodit

PROG=		tfmtodit
SRCS=		tfmtodit.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

MANDEPEND=	tfmtodit.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
