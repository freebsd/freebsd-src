# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/tbl/Makefile,v 1.1.2.2 2002/07/19 08:34:10 ru Exp $

PROG_CXX=	tbl
SRCS=		main.cc table.cc
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
