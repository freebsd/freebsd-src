# $FreeBSD$

PROG=		grohtml
SRCS=		html.cc output.cc

LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	grohtml.1

.include "../Makefile.cfg"
.include <bsd.prog.mk>
