# $FreeBSD$

PROG=		hpftodit
SRCS=		hpftodit.cc
DPADD=		${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
