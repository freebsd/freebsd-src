# $FreeBSD: src/sbin/mount_nwfs/Makefile,v 1.1.2.2 2001/04/25 10:58:39 ru Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN=	mount_nwfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNWFS -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
