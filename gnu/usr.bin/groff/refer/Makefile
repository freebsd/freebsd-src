#	$Id: Makefile,v 1.6 1998/03/20 12:05:50 bde Exp $

PROG=		refer
SRCS=		command.cc label.cc ref.cc refer.cc token.cc
CFLAGS+=	-I${.CURDIR}/../include
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	label.cc label.tab.h ${MAN1} y.tab.c y.tab.h

.include "../Makefile.cfg"
.include <bsd.prog.mk>
