#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.39 2004/02/02 18:01:19 ru Exp $

PROG=	su

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
