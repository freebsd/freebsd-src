# $FreeBSD: src/usr.sbin/ntp/ntpdc/Makefile,v 1.11 2006/03/17 18:54:41 ru Exp $

NO_MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
SRCS= 	ntpdc.c ntpdc_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBMD}
LDADD=	${LIBNTP} -lmd

.if ${MK_GNU_SUPPORT} != "no"
DPADD+=		${LIBREADLINE} ${LIBTERMCAP}
LDADD+=		-lreadline -ltermcap
CFLAGS+=	-DHAVE_LIBREADLINE -DHAVE_READLINE_HISTORY_H \
		-DHAVE_READLINE_READLINE_H
.endif

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
