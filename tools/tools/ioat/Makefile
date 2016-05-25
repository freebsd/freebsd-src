# $FreeBSD$

PROG=	ioatcontrol
MAN=	ioatcontrol.8
CFLAGS+=	-I${.CURDIR:H:H:H}/sys/dev/ioat
WARNS?=	6
#LIBADD=	util
LDADD=	-lutil

.include <bsd.prog.mk>
