# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.15 2001/09/13 06:48:15 ru Exp $

S=	${.CURDIR}/..
.PATH: $S

PROG=	mrinfo
MAN=	mrinfo.8
BINOWN=	root
BINMODE= 4555

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

.include <bsd.prog.mk>
