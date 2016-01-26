#
# $FreeBSD$
#

PROG=	bhyvectl
SRCS=	bhyvectl.c

MAN=	bhyvectl.8

DPADD=	${LIBVMMAPI} ${LIBUTIL}
LDADD=	-lvmmapi -lutil

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
