# $FreeBSD: src/usr.sbin/ngctl/Makefile,v 1.3.2.2 1999/12/01 19:41:40 julian Exp $
# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

PROG=		ngctl
SRCS=		main.c mkpeer.c connect.c name.c show.c list.c \
		msg.c debug.c shutdown.c rmhook.c status.c types.c
MAN8=		ngctl.8
DPADD+=		${LIBNETGRAPH}
LDADD+=		-lnetgraph
COPTS+=		-Wall

.include <bsd.prog.mk>
