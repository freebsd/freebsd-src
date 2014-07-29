# $FreeBSD$

MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
SRCS= 	ntpdc.c ntpdc_ops.c ntpdc-opts.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts

DPADD=	${LIBNTP} ${LIBM} ${LIBMD} ${LIBOPTS}
LDADD=	${LIBNTP} -lm -lmd ${LIBOPTS}

DPADD+=		${LIBEDIT} ${LIBTERMCAPW}
LDADD+=		-ledit -ltermcapw
CFLAGS+=	-DHAVE_LIBEDIT -DHAVE_READLINE_READLINE_H \
		-I${DESTDIR}/${INCLUDEDIR}/edit

CLEANFILES+= .version version.c

NO_PIE=	yes

version.c:
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
