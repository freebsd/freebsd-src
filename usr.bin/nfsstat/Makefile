#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/nfsstat/Makefile,v 1.3 2001/12/10 21:13:06 dwmalone Exp $

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm

.include <bsd.prog.mk>
