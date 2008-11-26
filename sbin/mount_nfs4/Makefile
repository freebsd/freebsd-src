#	${.CURDIR}(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/mount_nfs4/Makefile,v 1.2.20.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	mount_nfs4
SRCS=	mount_nfs4.c getmntopts.c mounttab.c
MAN=	mount_nfs4.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
WARNS?=	2

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
