# $FreeBSD: src/gnu/usr.bin/groff/grohtml/Makefile,v 1.1 2000/01/12 13:14:27 asmodai Exp $
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
