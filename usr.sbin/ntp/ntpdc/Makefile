# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdc

PROG=	ntpdc
NOMAN=	yes
SRCS= 	ntpdc.c ntpdc_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=	${LIBNTP} -lreadline -ltermcap

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
