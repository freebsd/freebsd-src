#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: src/sbin/umount/Makefile,v 1.6.2.1 2001/04/25 10:58:54 ru Exp $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
MAN=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}
.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
