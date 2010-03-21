# $NetBSD: Makefile,v 1.2 2008/08/18 08:25:32 plunky Exp $
# $FreeBSD: src/usr.sbin/bluetooth/btpand/Makefile,v 1.1.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	btpand
MAN=	btpand.8
SRCS=	btpand.c bnep.c channel.c client.c event.c packet.c server.c sdp.c tap.c

WARNS?=	3

DPADD+=	${LIBBLUETOOTH} ${LIBSDP} ${LIBUTIL}
LDADD+=	-lbluetooth -lsdp -lutil

.include <bsd.prog.mk>
