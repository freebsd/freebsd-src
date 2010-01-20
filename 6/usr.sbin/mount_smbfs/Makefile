# $FreeBSD$

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
WARNS?=	6
MAN=	mount_smbfs.8

MOUNTDIR=	${.CURDIR}/../../sbin/mount
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMBFS -I${MOUNTDIR} -I${CONTRIBDIR}/include

LDADD=	-lsmb -lkiconv
DPADD=	${LIBSMB} ${LIBKICONV}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv (see the -E option).
#
NO_SHARED?=	NO

.PATH:	${CONTRIBDIR}/mount_smbfs

.include <bsd.prog.mk>
