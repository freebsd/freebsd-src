#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.38 2001/12/12 23:29:13 markm Exp $

PROG=	su

DPADD+= ${LIBUTIL} ${LIBPAM}
LDADD+= -lutil ${MINUSLPAM}

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
