#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mroute.c ipx.c route.c \
	unix.c atalk.c # iso.c ns.c tp_astring.c

CFLAGS+=-I/sys # -g
#.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm -lipx
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
