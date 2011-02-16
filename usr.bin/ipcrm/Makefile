# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.7.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
