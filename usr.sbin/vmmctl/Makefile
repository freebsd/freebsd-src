#
# $FreeBSD$
#

PROG=	vmmctl
SRCS=	vmmctl.c

NO_MAN=

DPADD=	${LIBVMMAPI}
LDADD=	-lvmmapi

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
