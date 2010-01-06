#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	w
SRCS=	fmt.c pr_time.c proc_compare.c w.c
MAN=	w.1 uptime.1
DPADD=	${LIBKVM} ${LIBULOG} ${LIBUTIL}
LDADD=	-lkvm -lulog -lutil
#BINGRP= kmem
#BINMODE=2555
LINKS=	${BINDIR}/w ${BINDIR}/uptime

WARNS?=	1

.PATH: ${.CURDIR}/../../bin/ps

.include <bsd.prog.mk>
