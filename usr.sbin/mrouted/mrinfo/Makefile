#	$Id: Makefile,v 1.8 1997/12/17 15:11:43 bde Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN8=	mrinfo.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
