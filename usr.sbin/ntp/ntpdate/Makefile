# $FreeBSD: src/usr.sbin/ntp/ntpdate/Makefile,v 1.6.2.1.2.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
NO_MAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBM} ${LIBMD}
LDADD=	${LIBNTP} -lm -lmd

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
