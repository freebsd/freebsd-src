#	$Id: Makefile,v 1.6 1998/03/20 12:04:59 bde Exp $

PROG=		pic
SRCS=		lex.cc main.cc object.cc pic.cc pic.tab.h \
		common.cc troff.cc tex.cc
CFLAGS+=	-I.
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN1} pic.cc pic.tab.h y.tab.c y.tab.h

pic.tab.h: pic.cc

.include "../Makefile.cfg"
.include <bsd.prog.mk>
