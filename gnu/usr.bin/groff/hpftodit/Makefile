# $FreeBSD: src/gnu/usr.bin/groff/hpftodit/Makefile,v 1.1.2.1 2000/12/07 09:57:08 ru Exp $

PROG=		hpftodit
SRCS=		hpftodit.cc
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
