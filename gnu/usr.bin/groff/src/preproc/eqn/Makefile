# $FreeBSD$

PROG_CXX=	eqn
SRCS=		eqn.cpp eqn_tab.h main.cpp lex.cpp \
		box.cpp limit.cpp list.cpp over.cpp text.cpp \
		script.cpp mark.cpp other.cpp delim.cpp sqrt.cpp pile.cpp special.cpp
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF}
LDADD=		${LIBGROFF}
SCRIPTS=	neqn
MAN=		eqn.1 neqn.1
CLEANFILES=	eqn.cpp eqn_tab.h ${SCRIPTS} ${MAN} y.tab.c y.tab.h

NO_PIE=	yes

eqn_tab.h: eqn.cpp

.include <bsd.prog.mk>
