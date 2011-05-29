# $FreeBSD$

PROG=	t1-bkpt

SRCS=	t1-bkpt.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
