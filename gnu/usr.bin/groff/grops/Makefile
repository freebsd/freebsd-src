# $FreeBSD: src/gnu/usr.bin/groff/grops/Makefile,v 1.5.8.1 2000/12/07 09:57:07 ru Exp $

PROG=		grops
SRCS=		ps.cc psrm.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
