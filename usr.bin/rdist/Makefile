#	@(#)Makefile	8.3 (Berkeley) 7/19/93

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat
CLEANFILES=y.tab.h

.include <bsd.prog.mk>
