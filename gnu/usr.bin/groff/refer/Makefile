# Makefile for refer

PROG=		refer
SRCS=		command.cc ref.cc refer.cc token.cc
OBJS=		label.o
CFLAGS+=	-I. -I${.CURDIR}/../include
LDADD+=		${LIBBIB} ${LIBGROFF} -lm
DPADD+=		${LIBBIB} ${LIBGROFF} ${LIBMATH}

MANDEPEND=	refer.1
CLEANFILES+=	label.cc label.tab.h ${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
