# $FreeBSD: src/sbin/mount_reiserfs/Makefile,v 1.1 2005/05/24 12:34:45 dumbbell Exp $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}

.PATH: ${MOUNT}

.include <bsd.prog.mk>
