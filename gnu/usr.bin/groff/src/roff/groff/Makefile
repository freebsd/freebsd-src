# $FreeBSD$

PROG_CXX=	groff
SRCS=		groff.cc pipeline.c
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
