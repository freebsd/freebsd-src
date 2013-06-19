# $FreeBSD$

PROG=	bhyveload
SRCS=	bhyveload.c
MAN=	bhyveload.8

DPADD+=	${LIBVMMAPI}
LDADD+=	-lvmmapi

WARNS?=	3

CFLAGS+=-I${.CURDIR}/../../sys/boot/userboot

.include <bsd.prog.mk>
