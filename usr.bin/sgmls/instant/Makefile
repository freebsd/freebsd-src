#	$Id: Makefile,v 1.1.1.1 1996/09/08 01:55:10 jfieber Exp $

PROG=	instant
SRCS=	browse.c info.c main.c tables.c traninit.c translate.c 
SRCS+=	tranvar.c util.c

CFLAGS+= -I${.CURDIR}/../libsgmls -I${.CURDIR}/../sgmls

LDADD=  ${LIBSGMLS} -lcompat
DPADD=  ${LIBSGMLS} ${LIBCOMPAT}

MAN1=	instant.1
MAN5=	transpec.5

.include <bsd.prog.mk>
