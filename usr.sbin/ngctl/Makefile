# $FreeBSD: src/usr.sbin/ngctl/Makefile,v 1.4.2.2 2001/04/25 12:10:21 ru Exp $
# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

PROG=		ngctl
SRCS=		main.c mkpeer.c config.c connect.c name.c show.c list.c \
		msg.c debug.c shutdown.c rmhook.c status.c types.c
MAN=		ngctl.8
DPADD+=		${LIBNETGRAPH}
LDADD+=		-lnetgraph
COPTS+=		-Wall

.include <bsd.prog.mk>
