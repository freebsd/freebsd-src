# $FreeBSD$

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys -Wall
.PATH:	${MOUNT}
WARNS= 0

.include <bsd.prog.mk>
