# $FreeBSD: src/usr.sbin/ntp/ntptimeset/Makefile,v 1.2.2.1 2001/12/21 17:42:35 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpdate

PROG=	ntptimeset
NOMAN=	yes
SRCS=	ntptimeset.c ntptime_config.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntptimeset

.include <bsd.prog.mk>
