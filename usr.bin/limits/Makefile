#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	limits
SRCS=	limits.c

CFLAGS+=-Wall
LDADD+= -lutil
DPADD+= ${LIBUTIL}

BINOWN=	root
BINMODE=0555

.include <bsd.prog.mk>
