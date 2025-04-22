PROG=	mount_smbfs
PACKAGE=	smbutils
MAN=	mount_smbfs.8

CONTRIBDIR=	${SRCTOP}/contrib/smbfs
CFLAGS+=	-DSMBFS -I${CONTRIBDIR}/include

LIBADD=	smb util

.PATH:	${CONTRIBDIR}/mount_smbfs

.include <bsd.prog.mk>
