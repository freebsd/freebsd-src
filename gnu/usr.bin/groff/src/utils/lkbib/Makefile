# $FreeBSD$

PROG=		lkbib
SRCS=		lkbib.cc
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBBIB} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
