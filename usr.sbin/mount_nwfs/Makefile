# $FreeBSD: src/usr.sbin/mount_nwfs/Makefile,v 1.7.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN=	mount_nwfs.8

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -DNWFS -I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
