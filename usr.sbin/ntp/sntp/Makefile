# $FreeBSD: src/usr.sbin/ntp/sntp/Makefile,v 1.1 2004/07/20 15:45:04 roberto Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NOMAN=	yes
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\"

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
