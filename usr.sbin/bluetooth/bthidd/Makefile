# $Id: Makefile,v 1.3 2004/08/17 21:49:46 max Exp $
# $FreeBSD$

PROG=		bthidd
#MAN=		bthidd.8 bthidd.conf.5
NO_MAN=		1
SRCS=		bthidd.c client.c hid.c kbd.c lexer.l parser.y server.c \
		session.c

CFLAGS+=	-I${.CURDIR}
WARNS?=		2
DEBUG_FLAGS=	-g

DPADD=          ${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lusbhid

.include <bsd.prog.mk>
