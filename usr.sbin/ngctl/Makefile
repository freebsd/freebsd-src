# $FreeBSD$
# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

PROG=	ngctl
MAN=	ngctl.8
SRCS=	main.c mkpeer.c config.c connect.c name.c show.c list.c \
	msg.c debug.c shutdown.c rmhook.c status.c types.c write.c

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.prog.mk>
