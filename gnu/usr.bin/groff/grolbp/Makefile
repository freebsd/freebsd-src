# $FreeBSD: src/gnu/usr.bin/groff/grolbp/Makefile,v 1.1.2.1 2000/12/07 09:57:06 ru Exp $

PROG=		grolbp
SRCS=		lbp.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
