# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.6.24.1.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
