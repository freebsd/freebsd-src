#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: src/sbin/umount/Makefile,v 1.11.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
WARNS?=	0
MAN=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
