# $FreeBSD$

PROG=		grops
SRCS=		ps.cc psrm.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
