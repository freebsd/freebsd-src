# $Id: Makefile,v 1.4 2002/09/04 21:29:58 max Exp $
# $FreeBSD$

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man

PROG=		hcseriald
MAN8=		hcseriald.8

WARNS?=		2
CFLAGS+=	-Wall -O2 -I../../../sys/netgraph/bluetooth/include
SRCS=		hcseriald.c

DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph

.include <bsd.prog.mk>
