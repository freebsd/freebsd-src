# $FreeBSD: src/usr.sbin/ntp/ntp-genkeys/Makefile,v 1.1.2.1 2001/12/21 17:42:34 roberto Exp $

.PATH:	${.CURDIR}/../../../contrib/ntp/util \
	${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-genkeys
NOMAN=	yes
SRCS=	ntp-genkeys.c ntp_config.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
