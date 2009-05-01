#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_cd9660/Makefile,v 1.11.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	mount_cd9660
SRCS=	mount_cd9660.c getmntopts.c
MAN=	mount_cd9660.8
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
