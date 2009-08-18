#	$OpenBSD: Makefile,v 1.6 2003/11/20 23:23:09 avsm Exp $

CFLAGS+=-Wall -Wmissing-prototypes -Wshadow
LDADD+= -lpcap -lutil
DPADD+=	${LIBPCAP} ${LIBUTIL}

PROG=	pflogd
SRCS=	pflogd.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

.include <bsd.prog.mk>
