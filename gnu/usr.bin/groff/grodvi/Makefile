# Makefile for grodvi

PROG=		grodvi
SRCS=		dvi.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBMATH}

MANDEPEND=	grodvi.1
CLEANFILES+=	${MANDEPEND}

.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
.include <bsd.prog.mk>
