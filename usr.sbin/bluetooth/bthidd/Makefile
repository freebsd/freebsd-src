# $Id: Makefile,v 1.2 2004/02/13 21:46:20 max Exp $
# $FreeBSD$

PROG=		bthidd
#MAN=		bthidd.8 bthidd.conf.5
NOMAN=
SRCS=		bthidd.c client.c hid.c lexer.l parser.y server.c session.c
WARNS?=		1
CFLAGS+=	-I${.CURDIR}

DPADD=          ${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lusbhid

.include <bsd.prog.mk>
