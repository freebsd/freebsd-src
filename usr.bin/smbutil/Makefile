# $FreeBSD: src/usr.bin/smbutil/Makefile,v 1.1 2001/12/14 11:41:22 sheldonh Exp $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB}
LDADD=	-lsmb

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
