# $FreeBSD: src/gnu/usr.bin/groff/pic/Makefile,v 1.7.2.1 2000/12/07 09:57:18 ru Exp $

PROG=		pic
SRCS=		lex.cc main.cc object.cc pic.cc pic_tab.h \
		common.cc troff.cc tex.cc
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN1} pic.cc pic_tab.h y.tab.c y.tab.h

pic_tab.h: pic.cc

.include "../Makefile.cfg"
.include <bsd.prog.mk>

INCLUDES+=	-I${DIST_DIR} -I.
