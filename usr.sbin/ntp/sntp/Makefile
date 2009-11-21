# $FreeBSD: src/usr.sbin/ntp/sntp/Makefile,v 1.4.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NO_MAN=
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\" \
	-I${.CURDIR}/../../../contrib/ntp/libopts

DPADD=	${LIBM} ${LIBOPTS}
LDADD=	-lm ${LIBOPTS}

.include <bsd.prog.mk>
