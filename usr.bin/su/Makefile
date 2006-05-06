#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.40 2004/11/03 18:01:21 ru Exp $

PROG=	su

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
