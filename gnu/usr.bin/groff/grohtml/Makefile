# $FreeBSD$
#
# Makefile for grohtml

PROG=		grohtml
SRCS=		html.cc

LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

MANDEPEND=	grohtml.1
CLEANFILES+=	${MANDEPEND}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
