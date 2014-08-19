# $FreeBSD$

PROG_CXX=	tfmtodit
SRCS=		tfmtodit.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

NO_PIE=	yes

.include <bsd.prog.mk>
