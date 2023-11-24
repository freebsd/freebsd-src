
PROG=	mount_smbfs
PACKAGE=	smbutils
SRCS=	mount_smbfs.c getmntopts.c
MAN=	mount_smbfs.8

MOUNTDIR=	${SRCTOP}/sbin/mount
CONTRIBDIR=	${SRCTOP}/contrib/smbfs
CFLAGS+=	-DSMBFS -I${MOUNTDIR} -I${CONTRIBDIR}/include

LIBADD=	smb

.PATH:	${CONTRIBDIR}/mount_smbfs
.PATH:  ${MOUNTDIR}

.include <bsd.prog.mk>
