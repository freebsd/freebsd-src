#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: src/sbin/umount/Makefile,v 1.9 2001/12/04 02:19:58 obrien Exp $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
WARNS=	0
MAN=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}
WARNS=	0

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
