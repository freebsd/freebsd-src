# $FreeBSD$

PROG=		groff
SRCS=		groff.cc pipeline.c
DPADD=		${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
