# Makefile for addftinfo

PROG=		addftinfo
SRCS=		addftinfo.cc guess.cc
#CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF}
DPADD+=		${LIBGROFF}

MANDEPEND=	addftinfo.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
