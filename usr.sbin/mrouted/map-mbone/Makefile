#	$Id: Makefile,v 1.7 1998/03/07 09:49:03 bde Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	mapper.c
MAN8=	map-mbone.8

.include <bsd.prog.mk>
