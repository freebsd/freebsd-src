# $FreeBSD$

PROG=		grolj4
SRCS=		lj4.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
