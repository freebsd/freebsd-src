#
#	$Id: Makefile,v 1.5 1997/03/03 13:23:54 bde Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -D_NEW_VFSCONF
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
