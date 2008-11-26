# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/pic/Makefile,v 1.4.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	pic
SRCS=		lex.cpp main.cpp object.cpp pic.cpp pic_tab.h \
		common.cpp troff.cpp tex.cpp
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN} pic.cpp pic_tab.h y.tab.c y.tab.h

pic_tab.h: pic.cpp

.include <bsd.prog.mk>
