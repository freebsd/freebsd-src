# $FreeBSD$

PROG=		grotty
SRCS=		tty.cc
LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grotty.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
