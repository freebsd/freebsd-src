# $FreeBSD$

PROG=		iscsid
SRCS=		discovery.c iscsid.c keys.c log.c login.c pdu.c
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys/cam
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
#CFLAGS+=	-DICL_KERNEL_PROXY
MAN=		iscsid.8

DPADD=		${LIBUTIL}
LDADD=		-lcrypto -lssl -lutil

WARNS=		6

.include <bsd.prog.mk>
