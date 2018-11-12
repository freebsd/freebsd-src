# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntpdate
MAN=
SRCS=	ntpdate.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

LIBADD=	md rt ntp

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpdate

.include <bsd.prog.mk>
