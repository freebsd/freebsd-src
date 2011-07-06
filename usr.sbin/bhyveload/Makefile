# $FreeBSD$

PROG=	bhyveload
SRCS=	bhyveload.c
NO_MAN=

DPADD+=	${LIBVMMAPI}
LDADD+=	-lvmmapi

WARNS?=	3

CFLAGS+=-I${.CURDIR}/../../sys/boot/userboot

.include <bsd.prog.mk>
