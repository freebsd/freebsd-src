# $FreeBSD: src/usr.sbin/ntp/ntptimeset/Makefile,v 1.3 2001/07/20 06:20:06 obrien Exp $

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
