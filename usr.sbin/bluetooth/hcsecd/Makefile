# $Id: Makefile,v 1.2 2003/03/15 03:07:42 max Exp $
# $FreeBSD$

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		hcsecd
MAN8=		hcsecd.8
WARNS?=		1
CFLAGS+=	-g -I${.CURDIR} -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		hcsecd.c lexer.l parser.y

.include <bsd.prog.mk>
