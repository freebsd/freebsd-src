# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
NO_MAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBM} ${LIBMD} ${LIBRT}
LDADD=	${LIBNTP} -lm -lmd -lrt

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
