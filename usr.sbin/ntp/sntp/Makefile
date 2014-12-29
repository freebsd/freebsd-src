# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
MAN=	sntp.1
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\" \
	-I${.CURDIR}/../../../contrib/ntp/libopts

LIBADD=	m opts

.include <bsd.prog.mk>
