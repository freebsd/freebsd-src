# $FreeBSD$

PROG_CXX=	tbl
SRCS=		main.cc table.cc
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
