# Makefile for grotty

PROG=		grotty
SRCS=		tty.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

MANDEPEND=	grotty.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
