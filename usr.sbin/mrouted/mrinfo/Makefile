#	$Id: Makefile,v 1.9 1998/03/07 09:49:06 bde Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN8=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
