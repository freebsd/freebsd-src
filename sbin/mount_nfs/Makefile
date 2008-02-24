#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/mount_nfs/Makefile,v 1.16 2007/01/23 09:14:33 rodrigc Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c mounttab.c
MAN=	mount_nfs.8
MLINKS=	mount_nfs.8 mount_nfs4.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
WARNS?=	3

LINKS=	${BINDIR}/mount_nfs ${BINDIR}/mount_nfs4

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
