#	$Id: Makefile,v 1.10 1998/09/19 22:42:10 obrien Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN8=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
