# $FreeBSD: src/gnu/usr.bin/groff/tfmtodit/Makefile,v 1.5.8.1 2000/12/07 09:57:20 ru Exp $

PROG=		tfmtodit
SRCS=		tfmtodit.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
