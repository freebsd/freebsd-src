#
# $FreeBSD: src/sbin/mount_ntfs/Makefile,v 1.9 2004/02/23 20:13:54 johan Exp $
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
NOSHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
