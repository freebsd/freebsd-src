# $FreeBSD: src/gnu/usr.bin/groff/grolj4/Makefile,v 2.1.1.1.8.1 2000/12/07 09:57:07 ru Exp $

PROG=		grolj4
SRCS=		lj4.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
