# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.11.2.1 2001/04/25 12:10:11 ru Exp $

PROG=	mrinfo

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

MAN=	mrinfo.8
BINMODE=4555

.include <bsd.prog.mk>
