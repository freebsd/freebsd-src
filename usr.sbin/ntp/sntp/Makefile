# $FreeBSD: src/usr.sbin/ntp/sntp/Makefile,v 1.3 2004/12/21 08:47:34 ru Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NO_MAN=
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\"

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
