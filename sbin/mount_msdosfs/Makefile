#
# $FreeBSD: src/sbin/mount_msdosfs/Makefile,v 1.28.8.1 2009/04/15 03:14:26 kensmith Exp $
#

PROG=	mount_msdosfs
SRCS=	mount_msdosfs.c getmntopts.c
MAN=	mount_msdosfs.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS?=	6

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
