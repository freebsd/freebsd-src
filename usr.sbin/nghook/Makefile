# $FreeBSD$
# $Whistle: Makefile,v 1.4 1999/01/16 04:44:33 archie Exp $

PROG=		nghook
SRCS=		main.c
MAN8=		nghook.8
DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph
COPTS+=		-g -Wall -O2 -Werror

.include <bsd.prog.mk>
