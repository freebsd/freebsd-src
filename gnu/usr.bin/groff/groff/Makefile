# $FreeBSD$

PROG=		groff
SRCS=		groff.cc pipeline.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+=		${LIBGROFF} -lm
DPADD+=		${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
