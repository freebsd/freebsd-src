# $FreeBSD: src/gnu/usr.bin/groff/groff/Makefile,v 1.5.8.1 2000/12/07 09:57:03 ru Exp $

PROG=		groff
SRCS=		groff.cc pipeline.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
