# $FreeBSD: src/sbin/mount_reiserfs/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}
WARNS ?= 6

.PATH: ${MOUNT}

.include <bsd.prog.mk>
