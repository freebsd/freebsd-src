# $FreeBSD$

PROG=		eqn
SRCS=		eqn.cc eqn_tab.h main.cc lex.cc \
		box.cc limit.cc list.cc over.cc text.cc \
		script.cc mark.cc other.cc delim.cc sqrt.cc pile.cc special.cc
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF}
LDADD=		${LIBGROFF}
SCRIPTS=	neqn
MAN=		eqn.1 neqn.1
CLEANFILES=	eqn.cc eqn_tab.h ${SCRIPTS} ${MAN} y.tab.c y.tab.h

eqn_tab.h: eqn.cc

.include <bsd.prog.mk>
