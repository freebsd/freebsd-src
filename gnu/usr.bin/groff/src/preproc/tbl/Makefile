# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/tbl/Makefile,v 1.3 2002/04/11 11:06:05 ru Exp $

PROG_CXX=	tbl
SRCS=		main.cc table.cc
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
