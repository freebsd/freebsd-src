# $FreeBSD: src/usr.bin/smbutil/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB} ${LIBKICONV}
LDADD=	-lsmb -lkiconv

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
