#	$Id: Makefile,v 1.1.1.1 1996/01/16 05:14:09 jfieber Exp $

PROG=	instant
SRCS=	browse.c info.c main.c tables.c traninit.c translate.c 
SRCS+=	tranvar.c util.c

CFLAGS+= -I${.CURDIR}/../libsgmls -I${.CURDIR}/../sgmls

LDADD=  ${LIBSGMLS} -lcompat
DPADD=  ${LIBSGMLS}

MAN1=	instant.1
MAN5=	transpec.5

.include <bsd.prog.mk>
