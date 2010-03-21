# $FreeBSD: src/usr.sbin/ntp/ntpq/Makefile,v 1.11.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

NO_MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
SRCS= 	ntpq.c ntpq-opts.c ntpq-subs.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts

DPADD=	${LIBNTP} ${LIBM} ${LIBMD} ${LIBOPTS}
LDADD=	${LIBNTP} -lm -lmd ${LIBOPTS}

.if ${MK_GNU_SUPPORT} != "no"
DPADD+=		${LIBREADLINE} ${LIBTERMCAP}
LDADD+=		-lreadline -ltermcap
CFLAGS+=	-DHAVE_LIBREADLINE -DHAVE_READLINE_HISTORY_H \
		-DHAVE_READLINE_READLINE_H
.endif

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
