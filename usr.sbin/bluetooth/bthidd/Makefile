# $Id: Makefile,v 1.2 2004/02/13 21:46:20 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/bthidd/Makefile,v 1.1 2004/04/10 00:18:00 emax Exp $

PROG=		bthidd
#MAN=		bthidd.8 bthidd.conf.5
NOMAN=		1
SRCS=		bthidd.c client.c hid.c lexer.l parser.y server.c session.c
WARNS?=		1
CFLAGS+=	-I${.CURDIR}

DPADD=          ${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lusbhid

.include <bsd.prog.mk>
