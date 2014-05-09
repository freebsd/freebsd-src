# $FreeBSD$

PROG=		ctld
SRCS=		ctld.c discovery.c kernel.c keys.c log.c login.c parse.y pdu.c token.l y.tab.h
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys
CFLAGS+=	-I${.CURDIR}/../../sys/cam/ctl
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
#CFLAGS+=	-DICL_KERNEL_PROXY
MAN=		ctld.8 ctl.conf.5

DPADD=		${LIBCAM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=		-lbsdxml -lcam -lcrypto -lfl -lsbuf -lssl -lutil

YFLAGS+=	-v
CLEANFILES=	y.tab.c y.tab.h y.output

WARNS=		6
NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
