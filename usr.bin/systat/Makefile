#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	systat
CFLAGS+=-I${.CURDIR}/../../sys -I${.CURDIR}/../vmstat
SRCS=	cmds.c cmdtab.c disks.c fetch.c iostat.c keyboard.c vmstat.c main.c \
	mbufs.c netcmds.c netstat.c pigs.c swap.c
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBM} ${LIBKVM}
LDADD=	-lcurses -ltermcap -lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
