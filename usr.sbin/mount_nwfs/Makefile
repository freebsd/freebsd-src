# $FreeBSD: src/usr.sbin/mount_nwfs/Makefile,v 1.6 2003/07/02 16:43:13 gordon Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN=	mount_nwfs.8

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -DNWFS -I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
