# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-include ${.CURDIR}/../../lib/libpcap/config.h

LDADD=	-lpcap
DPADD=	${LIBPCAP} ${LIBUTIL}

WARNS?=	2

.include <bsd.prog.mk>
