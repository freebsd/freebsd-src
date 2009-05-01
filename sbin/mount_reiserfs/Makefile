# $FreeBSD: src/sbin/mount_reiserfs/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}
WARNS ?= 6

.PATH: ${MOUNT}

.include <bsd.prog.mk>
