#	$Id: Makefile,v 1.3 1994/03/30 09:31:19 jkh Exp $

DEBUG_FLAGS = -DDEBUGFSM -DDEBUGLCP -DDEBUGIPCP -DDEBUGUPAP -DDEBUGCHAP \
	-DDEBUGMAIN
CFLAGS+= ${DEBUG_FLAGS}

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c \
	auth.c options.c sys-bsd.c
MAN8=	pppd.8
BINMODE=4555
BINGRP=	daemon
BINOWN=	root

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

md5driver: md5.h md5.o md5driver.o
	$(CC) $(CFLAGS) -o md5driver md5driver.o md5.o

.include <bsd.prog.mk>
