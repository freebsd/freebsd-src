# $FreeBSD$

PROG=		tbl
SRCS=		main.cc table.cc
DPADD=		${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
