# $Id: Makefile,v 1.2 2003/03/15 03:07:45 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/hcseriald/Makefile,v 1.2 2003/05/10 21:50:35 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man

PROG=		hcseriald
MAN8=		hcseriald.8

WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		hcseriald.c

DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph

.include <bsd.prog.mk>
