# $Id: Makefile,v 1.1 2004/01/20 21:27:55 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/sdpd/Makefile,v 1.2 2005/01/05 18:37:37 emax Exp $

PROG=		sdpd
MAN=		sdpd.8
SRCS=		bgd.c dun.c ftrn.c irmc.c irmc_command.c lan.c log.c main.c \
		opush.c profile.c provider.c sar.c scr.c sd.c server.c sp.c \
		srr.c ssar.c ssr.c sur.c uuid.c

CFLAGS+=	-I${.CURDIR}
WARNS?=		2

.include <bsd.prog.mk>
