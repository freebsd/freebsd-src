# $FreeBSD: src/gnu/usr.bin/groff/grotty/Makefile,v 1.5.8.1 2000/12/07 09:57:08 ru Exp $

PROG=		grotty
SRCS=		tty.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
