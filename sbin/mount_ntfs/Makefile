#
# $FreeBSD: src/sbin/mount_ntfs/Makefile,v 1.10.20.1 2009/04/15 03:14:26 kensmith Exp $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	0

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
