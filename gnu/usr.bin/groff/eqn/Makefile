# $FreeBSD$

PROG=		eqn
SRCS=		eqn.cc eqn_tab.h main.cc lex.cc \
		box.cc limit.cc list.cc over.cc text.cc \
		script.cc mark.cc other.cc delim.cc sqrt.cc pile.cc special.cc
DPADD=		${LIBGROFF}
LDADD=		${LIBGROFF}
MAN1=		eqn.1 neqn.1
CLEANFILES=	eqn.cc eqn_tab.h ${MAN1} neqn y.tab.c y.tab.h

all: neqn

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} neqn \
	    ${DESTDIR}${BINDIR}

eqn_tab.h: eqn.cc

.include "../Makefile.cfg"
.include <bsd.prog.mk>

INCLUDES+=	-I${DIST_DIR} -I.

neqn: ${DIST_DIR}/neqn.sh
	sed -e 's/@g@/${g}/g' ${.ALLSRC} > ${.TARGET}
