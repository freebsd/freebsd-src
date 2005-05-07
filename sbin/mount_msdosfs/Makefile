#
# $FreeBSD: src/sbin/mount_msdosfs/Makefile,v 1.26 2003/10/23 16:09:20 trhodes Exp $
#

PROG=	mount_msdosfs
SRCS=	mount_msdosfs.c getmntopts.c
MAN=	mount_msdosfs.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS?=	2

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NOSHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
