# Makefile for tbl

PROG=		tbl
SRCS=		main.cc table.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBMATH}

MANDEPEND=	tbl.1
CLEANFILES+=	${MANDEPEND}

.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
.include <bsd.prog.mk>
