#	${.CURDIR}(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD$

PROG=	mount_nfs4
SRCS=	mount_nfs4.c getmntopts.c mounttab.c
MAN=	mount_nfs4.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
WARNS=	2

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
