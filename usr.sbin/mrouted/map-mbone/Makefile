# $FreeBSD$

S=	${.CURDIR}/..
.PATH: $S
	
PROG=	map-mbone
MAN=	map-mbone.8
SRCS=	mapper.c

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

.include <bsd.prog.mk>
