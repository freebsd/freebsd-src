# $FreeBSD$

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD} ${LIBUTIL}
LDADD=	-lpthread -lutil

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
