#
# $FreeBSD: src/usr.sbin/ntp/ntpdc/Makefile,v 1.3 2000/01/01 23:57:59 roberto Exp $
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpdc

PROG= ntpdc

SRCS= 	ntpdc.c ntpdc_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdc

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpdc

.include <bsd.prog.mk>
