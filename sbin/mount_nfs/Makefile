#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/mount_nfs/Makefile,v 1.18.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c mounttab.c
MAN=	mount_nfs.8
MLINKS=	mount_nfs.8 mount_newnfs.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
WARNS?=	3

LINKS=	${BINDIR}/mount_nfs ${BINDIR}/mount_newnfs

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
