#	@(#)Makefile	5.14 (Berkeley) 6/18/90

PROG=	netstat
SRCS=	inet.c if.c main.c mbuf.c route.c unix.c mroute.c

#SRCS+=	host.c
#CFLAGS+= -DIMP

SRCS+=	ns.c
CFLAGS+= -DNS

SRCS+=	iso.c
CFLAGS+= -DISO

BINGRP=	kmem
BINMODE=2555
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
