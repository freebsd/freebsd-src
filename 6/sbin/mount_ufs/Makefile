# $FreeBSD$

PROG=	mount_ufs
SRCS=	mount_ufs.c getmntopts.c
MAN=	mount_ufs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys -Wall
.PATH:	${MOUNT}
WARNS?= 1

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.include <bsd.prog.mk>
