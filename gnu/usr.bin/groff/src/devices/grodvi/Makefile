# $FreeBSD$

PROG=		grodvi
SRCS=		dvi.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
