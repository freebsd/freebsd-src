# $FreeBSD: src/usr.sbin/ntp/ntpdate/Makefile,v 1.4 2004/07/20 15:44:31 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
NOMAN=	yes
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBMD}
LDADD=	${LIBNTP} -lmd

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
