# $Id: Makefile,v 1.7 2001/04/16 04:34:26 bp Exp $

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
MAN8=	mount_smbfs.8

BINDIR=	/sbin
#NOSHARED=yes

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DSMBFS -I${MOUNT}

.PATH:	${MOUNT}

LDADD+= -lsmb
DPADD+= ${LIBSMB}

.include <bsd.prog.mk>
