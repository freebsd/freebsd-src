# $FreeBSD$

PROG=	netdumpsrv
MAN=	netdumpsrv.8

CFLAGS+=	-DNDEBUG -I${.CURDIR}

WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
