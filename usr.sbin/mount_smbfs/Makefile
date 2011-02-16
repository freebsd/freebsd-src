# $FreeBSD: src/usr.sbin/mount_smbfs/Makefile,v 1.8.14.1 2010/12/21 17:10:29 kensmith Exp $

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
.PATH:  ${MOUNTDIR}

.include <bsd.prog.mk>
