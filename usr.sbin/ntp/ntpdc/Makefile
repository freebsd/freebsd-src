# $FreeBSD$

NO_MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
SRCS= 	ntpdc.c ntpdc_ops.c ntpdc-opts.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts

DPADD=	${LIBNTP} ${LIBM} ${LIBMD} ${LIBOPTS}
LDADD=	${LIBNTP} -lm -lmd ${LIBOPTS}

DPADD+=		${LIBEDIT} ${LIBTERMCAP}
LDADD+=		-ledit -ltermcap
CFLAGS+=	-DHAVE_LIBEDIT -DHAVE_READLINE_READLINE_H \
		-I${DESTDIR}/${INCLUDEDIR}/edit

CLEANFILES+= .version version.c

version.c:
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

.include <bsd.prog.mk>
