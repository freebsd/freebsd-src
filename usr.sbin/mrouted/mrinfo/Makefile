# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.11 1999/08/28 01:17:13 peter Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN8=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
