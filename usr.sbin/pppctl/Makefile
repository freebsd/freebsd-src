#	$Id: Makefile,v 1.4 1998/08/31 12:14:32 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
