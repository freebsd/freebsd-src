#	$FreeBSD: src/sbin/mount_nwfs/Makefile,v 1.1 1999/10/15 09:59:48 bp Exp $

PROG=	mount_nwfs
SRCS=	mount_nwfs.c getmntopts.c
MAN8=	mount_nwfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNWFS -I${MOUNT}

.PATH:	${MOUNT}

LDADD+= -lncp -lipx
DPADD+= ${LIBIPX} ${LIBNCP}

.include <bsd.prog.mk>
