# $FreeBSD: src/sbin/mount_smbfs/Makefile,v 1.2 2002/12/30 09:38:46 obrien Exp $

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
WARNS?=	0
MAN=	mount_smbfs.8

MOUNTDIR=	${.CURDIR}/../mount
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMBFS -I${MOUNTDIR} -I${CONTRIBDIR}/include

LDADD+= -lsmb
DPADD+= ${LIBSMB}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv (see the -E option).
#
NOSHARED?=	NO

.PATH:	${CONTRIBDIR}/mount_smbfs

.include <bsd.prog.mk>
