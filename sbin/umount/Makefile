#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: src/sbin/umount/Makefile,v 1.6 1999/11/22 04:23:11 dillon Exp $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
MAN8=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}
.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
