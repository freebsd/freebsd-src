# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.10.2.1 1999/08/29 15:44:07 peter Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN8=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
