#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	su

DPADD+= ${LIBUTIL} ${LIBPAM}
LDADD+= -lutil ${MINUSLPAM}

# Cannot be enabled because of a truly nasty warning/problem
# concering execv(2)'s second argument.
#WARNS?=	2

BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
