# Makefile for groff

PROG=		groff
SRCS=		groff.cc pipeline.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBMATH}

MANDEPEND=	groff.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
