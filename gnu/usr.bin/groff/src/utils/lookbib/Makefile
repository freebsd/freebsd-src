# $FreeBSD$

PROG=		lookbib
SRCS=		lookbib.cc
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBBIB} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
