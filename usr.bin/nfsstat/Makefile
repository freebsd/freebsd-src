# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm

.include <bsd.prog.mk>
