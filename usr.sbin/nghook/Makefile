# $FreeBSD: src/usr.sbin/nghook/Makefile,v 1.2.2.1 1999/11/17 07:06:03 julian Exp $
# $Whistle: Makefile,v 1.4 1999/01/16 04:44:33 archie Exp $

PROG=		nghook
SRCS=		main.c
MAN8=		nghook.8
DPADD=		${LIBNETGRAPH}
LDADD=		-lnetgraph
COPTS+=		-Wall

.include <bsd.prog.mk>
