# $FreeBSD: src/sbin/mount_nwfs/Makefile,v 1.1.2.1 2000/07/01 07:42:36 ps Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN8=	mount_nwfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNWFS -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
