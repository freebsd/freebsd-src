#	$Id: Makefile,v 1.1 1997/06/28 01:04:49 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall -Wmissing-prototypes
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
