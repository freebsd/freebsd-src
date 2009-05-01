# $Id: Makefile,v 1.8 2003/08/14 20:06:20 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/hcsecd/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=		hcsecd
MAN=		hcsecd.8 hcsecd.conf.5
SRCS=		hcsecd.c lexer.l parser.y
WARNS?=		2
CFLAGS+=	-I${.CURDIR}

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
