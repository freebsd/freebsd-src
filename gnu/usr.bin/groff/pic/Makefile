# $FreeBSD$

PROG=		pic
SRCS=		lex.cc main.cc object.cc pic.cc pic_tab.h \
		common.cc troff.cc tex.cc
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	pic.1 pic.cc pic_tab.h y.tab.c y.tab.h

pic_tab.h: pic.cc

.include "../Makefile.cfg"
.include <bsd.prog.mk>

INCLUDES+=	-I${DIST_DIR} -I.
