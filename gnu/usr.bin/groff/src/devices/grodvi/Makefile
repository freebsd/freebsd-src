# $FreeBSD$

PROG_CXX=	grodvi
SRCS=		dvi.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

NO_PIE=	yes

.include <bsd.prog.mk>
