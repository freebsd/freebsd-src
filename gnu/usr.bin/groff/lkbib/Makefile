# $FreeBSD: src/gnu/usr.bin/groff/lkbib/Makefile,v 1.5.8.1 2000/12/07 09:57:10 ru Exp $

PROG=		lkbib
SRCS=		lkbib.cc
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBBIB} ${LIBGROFF} -lm
DPADD+=		${LIBBIB} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
