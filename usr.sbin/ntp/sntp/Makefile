# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NOMAN=
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\"

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
