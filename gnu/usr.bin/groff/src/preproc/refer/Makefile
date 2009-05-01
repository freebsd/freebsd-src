# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/refer/Makefile,v 1.4.32.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	refer
SRCS=		command.cpp label.cpp ref.cpp refer.cpp token.cpp
CFLAGS+=	-I${DIST_DIR}
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	label.cpp label_tab.h ${MAN} y.tab.c y.tab.h

.include <bsd.prog.mk>
