# Makefile for eqn

PROG=		eqn
MAN1=		eqn.1 neqn.1
SRCS=		main.cc lex.cc  box.cc limit.cc list.cc over.cc text.cc\
		script.cc mark.cc other.cc delim.cc sqrt.cc pile.cc special.cc
OBJS=		eqn.o
#CFLAGS+=	-I. -I${.CURDIR}/../include
LDADD+=		${LIBGROFF}
DPADD+=		${LIBGROFF}

MANDEPEND=	neqn.1 eqn.1
CLEANFILES+=	eqn.cc eqn.tab.h neqn ${MANDEPEND}

neqn:
	sed -e 's/@g@/${g}/g' ${DIST_DIR}/neqn.sh > neqn

afterinstall:	neqn
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} neqn \
		${DESTDIR}${BINDIR}

beforedepend: eqn.cc

.include "../Makefile.cfg"
.include <bsd.prog.mk>
