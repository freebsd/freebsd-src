# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.17.24.1 2008/10/02 02:57:24 kensmith Exp $

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
