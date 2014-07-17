# $FreeBSD$

MAN=

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
SRCS= 	ntpq.c ntpq-opts.c ntpq-subs.c version.c

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
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
