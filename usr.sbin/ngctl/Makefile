# $FreeBSD$
# $Whistle: Makefile,v 1.3 1999/01/16 00:10:11 archie Exp $

PROG=		ngctl
SRCS=		main.c mkpeer.c connect.c name.c show.c list.c \
		debug.c shutdown.c rmhook.c status.c types.c
MAN8=		ngctl.8

LDADD+=		-lnetgraph
DPADD+=		/usr/lib/libnetgraph.a

COPTS+=		-O2 -g -Wall -Werror
COPTS+=		-I/usr/src.freefall/sys

.include <bsd.prog.mk>
