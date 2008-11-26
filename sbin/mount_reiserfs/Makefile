# $FreeBSD: src/sbin/mount_reiserfs/Makefile,v 1.1.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}

.PATH: ${MOUNT}

.include <bsd.prog.mk>
