# $FreeBSD: src/usr.sbin/ntp/ntpq/Makefile,v 1.2.2.1 2001/12/21 17:42:35 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
NOMAN=	yes
SRCS= 	ntpq.c ntpq_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
