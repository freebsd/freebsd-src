# $FreeBSD: src/usr.sbin/mrouted/mrinfo/Makefile,v 1.17 2003/04/16 14:53:51 bde Exp $

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
