# $Id: Makefile,v 1.2 2003/03/15 03:06:53 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/bt3cfw/Makefile,v 1.2 2003/05/10 21:50:34 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		bt3cfw
MAN8=		bt3cfw.8
WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		bt3cfw.c

DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph

.include <bsd.prog.mk>
