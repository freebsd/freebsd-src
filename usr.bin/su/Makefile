#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su

DPADD+= ${LIBUTIL} ${LIBPAM}
LDADD+= -lutil ${MINUSLPAM}

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
