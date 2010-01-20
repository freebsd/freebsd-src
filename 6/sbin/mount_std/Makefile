#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#	$FreeBSD$

PROG=	mount_std
SRCS=	mount_std.c getmntopts.c
MAN=	mount_std.8
MLINKS=	mount_std.8 mount_devfs.8 \
	mount_std.8 mount_fdescfs.8 \
	mount_std.8 mount_linprocfs.8 \
	mount_std.8 mount_linsysfs.8 \
	mount_std.8 mount_procfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

LINKS=	${BINDIR}/mount_std ${BINDIR}/mount_devfs \
	${BINDIR}/mount_std ${BINDIR}/mount_fdescfs \
	${BINDIR}/mount_std ${BINDIR}/mount_linprocfs \
	${BINDIR}/mount_std ${BINDIR}/mount_linsysfs \
	${BINDIR}/mount_std ${BINDIR}/mount_procfs

.include <bsd.prog.mk>
