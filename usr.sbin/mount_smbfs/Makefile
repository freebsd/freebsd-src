# $FreeBSD$

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
WARNS?=	0
MAN=	mount_smbfs.8

MOUNTDIR=	${.CURDIR}/../../sbin/mount
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
