# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
NOMAN=
SRCS= 	ntpdc.c ntpdc_ops.c version.c

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
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
