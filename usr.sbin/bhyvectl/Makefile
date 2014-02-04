#
# $FreeBSD$
#

PROG=	bhyvectl
SRCS=	bhyvectl.c

NO_MAN=

DPADD=	${LIBVMMAPI} ${LIBUTIL}
LDADD=	-lvmmapi -lutil

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
