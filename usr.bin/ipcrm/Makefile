# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.6.24.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
