# Makefile for pfbtops

PROG=		pfbtops
SRCS=		pfbtops.c
LDADD+=		${LIBGROFF}
DPADD+=		${LIBGROFF}

MANDEPEND=	pfbtops.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
