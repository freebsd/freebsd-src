# Makefile for lkbib

PROG=		lkbib
SRCS=		lkbib.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBBIB} ${LIBGROFF} -lm
DPADD+=		${LIBBIB} ${LIBGROFF} ${LIBMATH}

MANDEPEND=	lkbib.1
CLEANFILES+=	${MANDEPEND}

.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
.include <bsd.prog.mk>
