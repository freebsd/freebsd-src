# $FreeBSD$

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
