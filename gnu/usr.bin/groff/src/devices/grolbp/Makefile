# $FreeBSD$

PROG=		grolbp
SRCS=		lbp.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
