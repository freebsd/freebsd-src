# $FreeBSD: src/usr.sbin/ntp/ntpdate/Makefile,v 1.4.2.1 2005/02/13 07:23:43 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
NOMAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBMD}
LDADD=	${LIBNTP} -lmd

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
