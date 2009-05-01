#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/w/Makefile,v 1.8.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	w
SRCS=	fmt.c pr_time.c proc_compare.c w.c
MAN=	w.1 uptime.1
DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil
#BINGRP= kmem
#BINMODE=2555
LINKS=	${BINDIR}/w ${BINDIR}/uptime

.PATH: ${.CURDIR}/../../bin/ps

.include <bsd.prog.mk>
