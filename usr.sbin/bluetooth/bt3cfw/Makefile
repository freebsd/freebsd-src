# $Id: Makefile,v 1.1.1.1 2002/11/12 00:39:18 max Exp $
# $FreeBSD$

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		bt3cfw
MAN8=		bt3cfw.8
WARNS?=		2
CFLAGS+=	-g -I../../../sys/netgraph/bluetooth/include
SRCS=		bt3cfw.c

DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph

.include <bsd.prog.mk>
