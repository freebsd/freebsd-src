# $FreeBSD$

PROG=		pic
SRCS=		lex.cc main.cc object.cc pic.cc pic_tab.h \
		common.cc troff.cc tex.cc
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN} pic.cc pic_tab.h y.tab.c y.tab.h

pic_tab.h: pic.cc

.include <bsd.prog.mk>
