# $FreeBSD: src/sbin/mount_udf/Makefile,v 1.2 2002/04/15 20:16:48 asmodai Exp $

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys -Wall
.PATH:	${MOUNT}
WARNS= 1

.include <bsd.prog.mk>
