# Makefile for pic

PROG=		pic
SRCS=		lex.cc main.cc object.cc common.cc troff.cc tex.cc
OBJS=		pic.o
CFLAGS+=	-I. -I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBMATH}

MANDEPEND=	pic.1
CLEANFILES+=	pic.cc pic.tab.h ${MANDEPEND}

beforedepend: pic.cc

.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
.include <bsd.prog.mk>
