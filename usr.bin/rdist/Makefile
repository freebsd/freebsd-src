#	@(#)Makefile	8.3 (Berkeley) 7/19/93

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c rshrcmd.c server.c
SRCS+=	gram.c
CLEANFILES=y.tab.h gram.c

# To use the old method, which requires setuid-root and all the baggage and
# security holes that goes with it, uncomment:
# CFLAGS+= -DDIRECT_RCMD
# BINOWN=	root
# BINMODE=4555
# INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
