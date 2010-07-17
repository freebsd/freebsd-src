# $FreeBSD: src/usr.bin/ipcrm/Makefile,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

.include <bsd.prog.mk>
