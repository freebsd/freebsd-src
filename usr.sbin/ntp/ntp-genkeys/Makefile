# $FreeBSD: src/usr.sbin/ntp/ntp-genkeys/Makefile,v 1.2 2003/04/04 17:49:16 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-genkeys
NOMAN=	yes
SRCS=	ntp-genkeys.c ntp_config.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

DPADD=	${LIBNTP}
LDADD=	${LIBNTP}

.include <bsd.prog.mk>
