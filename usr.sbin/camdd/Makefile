# $FreeBSD$

PROG=	camdd
SRCS=	camdd.c
SDIR=	${.CURDIR}/../../sys
DPADD=	${LIBCAM} ${LIBMT} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL} ${LIBTHR}
LDADD=	-lcam -lmt -lsbuf -lbsdxml -lutil -lthr
NO_WTHREAD_SAFETY=	1
MAN=	camdd.8

.include <bsd.prog.mk>
