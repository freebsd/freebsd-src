# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
NOMAN=	yes
SRCS= 	ntpdc.c ntpdc_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} -lreadline
LDADD=	${LIBNTP} -lreadline

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
