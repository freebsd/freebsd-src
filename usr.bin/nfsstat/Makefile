#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/nfsstat/Makefile,v 1.3.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm

.include <bsd.prog.mk>
