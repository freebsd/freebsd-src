#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/w/Makefile,v 1.6 1999/11/17 15:28:44 peter Exp $

PROG=	w
SRCS=	fmt.c pr_time.c proc_compare.c w.c
MAN1=	w.1 uptime.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
#BINGRP= kmem
#BINMODE=2555
LINKS=	${BINDIR}/w ${BINDIR}/uptime

.PATH: ${.CURDIR}/../../bin/ps

.include <bsd.prog.mk>
