# $FreeBSD: src/sbin/mount_nwfs/Makefile,v 1.5 2001/12/04 02:19:51 obrien Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN=	mount_nwfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNWFS -I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
