# Makefile for pfbtops

PROG=		pfbtops
SRCS=		pfbtops.c
LDADD+=		${LIBGROFF}
DPADD+=		${LIBGROFF}

MANDEPEND=	pfbtops.1
CLEANFILES+=	${MANDEPEND}

.include "../../../usr.bin/Makefile.inc"
.include "../Makefile.cfg"
.include <bsd.prog.mk>
