#	$Id$

PROG=		refer
SRCS=		command.cc label.cc ref.cc refer.cc token.cc
CFLAGS+=	-I${.CURDIR}/../include
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	label.cc label.tab.h ${MAN1} y.tab.c y.tab.h

.include "../Makefile.cfg"
.include <bsd.prog.mk>
