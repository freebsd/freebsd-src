# $FreeBSD$

PROG=	powerd
MAN=	powerd.8
WARNS?=	6
LDFLAGS+= -lpthread 

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
