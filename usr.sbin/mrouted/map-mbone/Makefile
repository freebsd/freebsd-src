#	$Id: Makefile,v 1.6 1997/12/17 15:11:42 bde Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	mapper.c
MAN8=	map-mbone.8

.include <bsd.prog.mk>
