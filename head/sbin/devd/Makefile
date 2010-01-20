# $FreeBSD$

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5

WARNS=	0
#WARNS?=	4

NO_SHARED?=YES

DPADD=	${LIBL} ${LIBUTIL}
LDADD=	-ll -lutil

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

CLEANFILES= y.output

.include <bsd.prog.mk>
