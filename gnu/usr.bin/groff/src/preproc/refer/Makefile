# $FreeBSD$

PROG_CXX=	refer
SRCS=		command.cpp label.cpp ref.cpp refer.cpp token.cpp
CFLAGS+=	-I${DIST_DIR}
DPADD=		${LIBBIB} ${LIBGROFF}
LDADD=		${LIBBIB} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	label.cpp label_tab.h ${MAN} y.tab.c y.tab.h

.include <bsd.prog.mk>
