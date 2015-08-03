# $FreeBSD$

PROG=		ctld
SRCS=		chap.c ctld.c discovery.c isns.c kernel.c keys.c log.c
SRCS+=		login.c parse.y pdu.c token.l y.tab.h
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys
CFLAGS+=	-I${.CURDIR}/../../sys/cam/ctl
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
#CFLAGS+=	-DICL_KERNEL_PROXY
MAN=		ctld.8 ctl.conf.5

DPADD=		${LIBBSDXML} ${LIBL} ${LIBMD} ${LIBSBUF} ${LIBUTIL}
LDADD=		-lbsdxml -ll -lmd -lsbuf -lutil

YFLAGS+=	-v
CLEANFILES=	y.tab.c y.tab.h y.output

WARNS=		6
NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
