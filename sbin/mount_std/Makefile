#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#	$FreeBSD: src/sbin/mount_std/Makefile,v 1.4.6.5 2003/02/10 12:21:07 des Exp $

PROG=	mount_std
SRCS=	mount_std.c getmntopts.c
MAN=	mount_std.8
MLINKS=	mount_std.8 mount_fdesc.8 \
	mount_std.8 mount_linprocfs.8 \
	mount_std.8 mount_procfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

LINKS=	${BINDIR}/mount_std ${BINDIR}/mount_fdesc \
	${BINDIR}/mount_std ${BINDIR}/mount_linprocfs \
	${BINDIR}/mount_std ${BINDIR}/mount_procfs

.include <bsd.prog.mk>
