# $FreeBSD: src/gnu/usr.bin/groff/grohtml/Makefile,v 1.1.4.1 2000/12/07 09:57:06 ru Exp $

PROG=		grohtml
SRCS=		html.cc output.cc

LDADD+=		${LIBDRIVER} ${LIBGROFF} -lm
DPADD+=		${LIBDRIVER} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
