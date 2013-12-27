# $FreeBSD$

PROG=	t3-name2sym

SRCS=	t3-name2sym.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
