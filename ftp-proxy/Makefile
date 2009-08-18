#	$OpenBSD: Makefile,v 1.2 2005/06/07 14:12:07 camield Exp $

PROG=	ftp-proxy
SRCS=	ftp-proxy.c filter.c
MAN=	ftp-proxy.8

CFLAGS+= -I${.CURDIR}
CFLAGS+= -Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
         -Wno-uninitialized
LDADD+=	-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
