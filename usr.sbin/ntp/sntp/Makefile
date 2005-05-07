# $FreeBSD: src/usr.sbin/ntp/sntp/Makefile,v 1.1.2.1 2005/02/13 07:23:44 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NOMAN=
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\"

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
