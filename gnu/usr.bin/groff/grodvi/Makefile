# $FreeBSD: src/gnu/usr.bin/groff/grodvi/Makefile,v 1.5.8.1 2000/12/07 09:57:02 ru Exp $

PROG=		grodvi
SRCS=		dvi.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
