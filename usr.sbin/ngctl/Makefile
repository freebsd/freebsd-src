# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

.include <src.opts.mk>

PROG=	ngctl
MAN=	ngctl.8
SRCS=	main.c mkpeer.c config.c connect.c dot.c name.c show.c list.c \
	msg.c debug.c shutdown.c rmhook.c status.c types.c write.c
WARNS?=	3

LIBADD=	netgraph

CFLAGS+= -DEDITLINE
LIBADD+=	edit pthread

.include <bsd.prog.mk>
