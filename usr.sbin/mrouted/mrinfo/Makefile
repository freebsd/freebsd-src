# $FreeBSD$

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
