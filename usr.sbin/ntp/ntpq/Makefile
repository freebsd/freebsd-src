# $FreeBSD: src/usr.sbin/ntp/ntpq/Makefile,v 1.4 2002/10/29 20:20:36 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpq

BINDIR=	/usr/bin

PROG=	ntpq
NOMAN=	yes
SRCS= 	ntpq.c ntpq_ops.c version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=	${LIBNTP} -lreadline -ltermcap

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpq

.include <bsd.prog.mk>
