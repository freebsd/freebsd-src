#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: src/sbin/umount/Makefile,v 1.11.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
WARNS?=	0
MAN=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
