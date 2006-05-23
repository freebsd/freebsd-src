# $FreeBSD: src/usr.sbin/ntp/ntpq/Makefile,v 1.8.2.1 2005/02/13 07:23:43 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
NOMAN=
SRCS= 	ntpq.c ntpq_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBMD}
LDADD=	${LIBNTP} -lmd

.if !defined(NO_GNU)
DPADD+=		${LIBREADLINE} ${LIBTERMCAP}
LDADD+=		-lreadline -ltermcap
CFLAGS+=	-DHAVE_LIBREADLINE -DHAVE_READLINE_HISTORY_H \
		-DHAVE_READLINE_READLINE_H
.endif

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
