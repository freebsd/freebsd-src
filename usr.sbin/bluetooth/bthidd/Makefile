# $Id: Makefile,v 1.2 2004/10/24 15:33:01 ru Exp $
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
